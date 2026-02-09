/**
 * FAT12/16 filesystem - read-only mount, find in root, read file by cluster chain.
 */

#include <kernel/fat.h>
#include <kernel/ata.h>
#include <kernel/types.h>

static struct fat_bpb bpb;
static uint32_t fat_start_lba;
static uint32_t fat_sectors;
static uint32_t root_start_lba;
static uint32_t root_sectors;
static uint32_t data_start_lba;
static uint8_t sectors_per_cluster;
static uint16_t bytes_per_sector;
static bool is_fat12;

static uint8_t sector_buf[512];
static uint8_t fat_sector_buf[512];

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static uint32_t get_fat_entry(uint32_t cluster)
{
    if (is_fat12) {
        uint32_t offset = cluster * 3 / 2;
        uint32_t sector = fat_start_lba + offset / bytes_per_sector;
        uint32_t off = offset % bytes_per_sector;
        if (ata_read_sectors(sector, 1, fat_sector_buf) != 0) return 0x0FFF;
        uint16_t w = *(uint16_t *)(fat_sector_buf + off);
        if (cluster & 1) return w >> 4; else return w & 0x0FFF;
    } else {
        uint32_t offset = cluster * 2;
        uint32_t sector = fat_start_lba + offset / bytes_per_sector;
        uint32_t off = offset % bytes_per_sector;
        if (ata_read_sectors(sector, 1, fat_sector_buf) != 0) return 0xFFFF;
        return *(uint16_t *)(fat_sector_buf + off);
    }
}

int fat_mount(void)
{
    if (ata_read_sectors(0, 1, sector_buf) != 0) return -1;
    struct fat_bpb *p = (struct fat_bpb *)sector_buf;
    if (p->bytes_per_sector != 512) return -1;
    bytes_per_sector = p->bytes_per_sector;
    sectors_per_cluster = p->sectors_per_cluster;
    root_sectors = (p->root_entries * FAT_ROOT_ENTRY_SIZE + bytes_per_sector - 1) / bytes_per_sector;
    fat_sectors = p->sectors_per_fat_16 ? p->sectors_per_fat_16 : 0;
    if (!fat_sectors) return -1;
    fat_start_lba = p->reserved_sectors;
    root_start_lba = fat_start_lba + p->num_fats * fat_sectors;
    data_start_lba = root_start_lba + root_sectors;
    uint32_t total = p->total_sectors_16 ? p->total_sectors_16 : p->total_sectors_32;
    uint32_t data_sectors = total - (p->reserved_sectors + p->num_fats * fat_sectors + root_sectors);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    is_fat12 = (total_clusters < 4085);
    return 0;
}

int fat_find_root(const char *name_8_3, uint32_t *out_cluster, uint32_t *out_size)
{
    uint32_t entries_per_sector = bytes_per_sector / FAT_ROOT_ENTRY_SIZE;
    for (uint32_t s = 0; s < root_sectors; s++) {
        if (ata_read_sectors(root_start_lba + s, 1, sector_buf) != 0) return -1;
        struct fat_dir_entry *e = (struct fat_dir_entry *)sector_buf;
        for (uint32_t i = 0; i < entries_per_sector; i++, e++) {
            if (e->name[0] == 0x00) return -1;
            if (e->name[0] == 0xE5) continue;
            if ((e->attr & FAT_ATTR_VOLUME_ID) || (e->attr & FAT_ATTR_DIR)) continue;
            size_t k = 0;
            for (; k < 11 && name_8_3[k]; k++)
                if ((uint8_t)e->name[k] != (uint8_t)name_8_3[k]) break;
            if (k == 11 || !name_8_3[k]) {
                uint32_t cluster = e->first_cluster_lo | ((uint32_t)e->first_cluster_hi << 16);
                *out_cluster = cluster;
                *out_size = e->size;
                return 0;
            }
        }
    }
    return -1;
}

int fat_read_file(uint32_t start_cluster, uint32_t size, void *buf)
{
    uint8_t *p = (uint8_t *)buf;
    uint32_t cluster = start_cluster;
    uint32_t read = 0;
    uint32_t cluster_size = sectors_per_cluster * bytes_per_sector;
    while (read < size && cluster >= 2) {
        uint32_t eoc_min = is_fat12 ? 0x0FF8u : 0xFFF8u;
        if (cluster >= eoc_min) break;
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster && read < size; s++) {
            if (ata_read_sectors(lba + s, 1, sector_buf) != 0) return (int)read;
            uint32_t chunk = bytes_per_sector;
            if (read + chunk > size) chunk = size - read;
            for (uint32_t i = 0; i < chunk; i++) p[read + i] = sector_buf[i];
            read += chunk;
        }
        cluster = get_fat_entry(cluster);
    }
    return (int)read;
}
