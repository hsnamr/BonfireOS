/**
 * FAT12/16 and exFAT — read-only mount, find in root, read file by cluster chain.
 */

#include <kernel/fat.h>
#include <kernel/ata.h>
#include <kernel/types.h>

enum { FS_NONE, FS_FAT1216, FS_EXFAT };

static uint8_t fs_kind;
static uint32_t fat_start_lba;
static uint32_t fat_sectors;
static uint32_t root_start_lba;
static uint32_t root_sectors;
static uint32_t data_start_lba;
static uint8_t sectors_per_cluster;
static uint16_t bytes_per_sector;
static bool is_fat12;

/* exFAT */
static uint32_t exfat_root_cluster;
/* Set by fat_find_root on exFAT when stream entry has NoFatChain (contiguous clusters). */
static bool exfat_nofatchain;

static uint8_t sector_buf[512];
static uint8_t fat_sector_buf[512];

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int memcmp_ex(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 1;
    return 0;
}

static char ascii_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static void build_expected_from_83(const char *n11, char *out, size_t outsz)
{
    size_t bi = 0;
    size_t i;
    for (i = 0; i < 8 && n11[i] != ' '; i++) {
        if (bi + 1 >= outsz) return;
        out[bi++] = ascii_upper(n11[i]);
    }
    size_t ext_start = 8;
    while (ext_start < 11 && n11[ext_start] == ' ') ext_start++;
    if (ext_start < 11) {
        if (bi + 1 >= outsz) return;
        out[bi++] = '.';
        for (i = ext_start; i < 11 && n11[i] != ' '; i++) {
            if (bi + 1 >= outsz) return;
            out[bi++] = ascii_upper(n11[i]);
        }
    }
    if (bi < outsz) out[bi] = 0;
}

static bool name_matches_expected(const char *expected, const uint16_t *w, int nchars)
{
    int ei = 0;
    for (int k = 0; k < nchars; k++) {
        uint16_t c = w[k];
        if (c == 0) return expected[ei] == 0;
        if (c > 127) return false;
        char ec = expected[ei];
        if (ec == 0) return false;
        if (ascii_upper((char)c) != ec) return false;
        ei++;
    }
    return expected[ei] == 0;
}

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static uint32_t exfat_cluster_to_lba(uint32_t cluster)
{
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static void copy_dentry_at(uint32_t cluster_first_lba, uint32_t off, uint8_t d[32])
{
    uint32_t sec = off / bytes_per_sector;
    uint32_t rel = off % bytes_per_sector;
    if (ata_read_sectors(cluster_first_lba + sec, 1, sector_buf) != 0) {
        for (int i = 0; i < 32; i++) d[i] = 0;
        return;
    }
    if (rel + 32 <= bytes_per_sector) {
        for (int i = 0; i < 32; i++) d[i] = sector_buf[rel + i];
    } else {
        uint32_t n1 = bytes_per_sector - rel;
        for (uint32_t i = 0; i < n1; i++) d[i] = sector_buf[rel + i];
        if (ata_read_sectors(cluster_first_lba + sec + 1, 1, sector_buf) != 0) return;
        for (uint32_t i = n1; i < 32; i++) d[i] = sector_buf[i - n1];
    }
}

static uint32_t get_fat_entry_fat1216(uint32_t cluster)
{
    if (is_fat12) {
        uint32_t offset = cluster * 3 / 2;
        uint32_t sector = fat_start_lba + offset / bytes_per_sector;
        uint32_t off = offset % bytes_per_sector;
        if (ata_read_sectors(sector, 1, fat_sector_buf) != 0) return 0x0FFF;
        uint16_t w = *(uint16_t *)(fat_sector_buf + off);
        if (cluster & 1) return w >> 4;
        else return w & 0x0FFF;
    } else {
        uint32_t offset = cluster * 2;
        uint32_t sector = fat_start_lba + offset / bytes_per_sector;
        uint32_t off = offset % bytes_per_sector;
        if (ata_read_sectors(sector, 1, fat_sector_buf) != 0) return 0xFFFF;
        return *(uint16_t *)(fat_sector_buf + off);
    }
}

static uint32_t get_fat_entry_exfat(uint32_t cluster)
{
    uint32_t offset = cluster * 4;
    uint32_t sector = fat_start_lba + offset / bytes_per_sector;
    uint32_t off = offset % bytes_per_sector;
    if (ata_read_sectors(sector, 1, fat_sector_buf) != 0) return 0xFFFFFFFFu;
    if (off + 4 <= bytes_per_sector) return rd32(fat_sector_buf + off);
    uint8_t tmp[4];
    uint32_t n0 = bytes_per_sector - off;
    for (uint32_t i = 0; i < n0; i++) tmp[i] = fat_sector_buf[off + i];
    if (ata_read_sectors(sector + 1, 1, fat_sector_buf) != 0) return 0xFFFFFFFFu;
    for (uint32_t i = n0; i < 4; i++) tmp[i] = fat_sector_buf[i - n0];
    return rd32(tmp);
}

static uint32_t get_fat_entry(uint32_t cluster)
{
    if (fs_kind == FS_EXFAT) return get_fat_entry_exfat(cluster);
    return get_fat_entry_fat1216(cluster);
}

static int exfat_mount(const uint8_t *boot)
{
    exfat_nofatchain = false;
    static const uint8_t sig_exfat[8] = { 'E', 'X', 'F', 'A', 'T', ' ', ' ', ' ' };
    if (memcmp_ex(boot + 3, sig_exfat, 8) != 0) return -1;
    if (rd16(boot + 510) != 0xAA55) return -1;

    uint8_t ss = boot[0x6C];
    uint8_t spc = boot[0x6D];
    if (ss < 9 || ss > 12) return -1;
    bytes_per_sector = (uint16_t)(1u << ss);
    if (bytes_per_sector != 512) return -1;
    sectors_per_cluster = (uint8_t)(1u << spc);

    fat_start_lba = rd32(boot + 0x50);
    fat_sectors = rd32(boot + 0x54);
    data_start_lba = rd32(boot + 0x58);
    exfat_root_cluster = rd32(boot + 0x60);
    (void)rd32(boot + 0x5C);

    if (exfat_root_cluster < 2) return -1;
    if (fat_sectors == 0 || data_start_lba == 0) return -1;

    fs_kind = FS_EXFAT;
    return 0;
}

static int fat1216_mount(const uint8_t *sector_buf_in)
{
    struct fat_bpb *p = (struct fat_bpb *)sector_buf_in;
    if (p->bytes_per_sector != 512) return -1;
    bytes_per_sector = p->bytes_per_sector;
    sectors_per_cluster = p->sectors_per_cluster;
    root_sectors =
        (p->root_entries * FAT_ROOT_ENTRY_SIZE + bytes_per_sector - 1) / bytes_per_sector;
    fat_sectors = p->sectors_per_fat_16 ? p->sectors_per_fat_16 : 0;
    if (!fat_sectors) return -1;
    fat_start_lba = p->reserved_sectors;
    root_start_lba = fat_start_lba + p->num_fats * fat_sectors;
    data_start_lba = root_start_lba + root_sectors;
    uint32_t total = p->total_sectors_16 ? p->total_sectors_16 : p->total_sectors_32;
    uint32_t data_sectors = total - (p->reserved_sectors + p->num_fats * fat_sectors + root_sectors);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    is_fat12 = (total_clusters < 4085);
    fs_kind = FS_FAT1216;
    return 0;
}

int fat_mount(void)
{
    fs_kind = FS_NONE;
    if (ata_read_sectors(0, 1, sector_buf) != 0) return -1;
    if (exfat_mount(sector_buf) == 0) return 0;
    return fat1216_mount(sector_buf);
}

static int exfat_find_root(const char *name_83, uint32_t *out_cluster, uint32_t *out_size)
{
    exfat_nofatchain = false;
    char expected[64];
    build_expected_from_83(name_83, expected, sizeof(expected));

    uint32_t dir_clu = exfat_root_cluster;
    uint32_t cluster_bytes = (uint32_t)sectors_per_cluster * bytes_per_sector;

    while (dir_clu >= 2 && dir_clu < 0xFFFFFFF8u) {
        uint32_t lba0 = exfat_cluster_to_lba(dir_clu);
        for (uint32_t off = 0; off + 32 <= cluster_bytes; ) {
            uint8_t d0[32];
            copy_dentry_at(lba0, off, d0);
            uint8_t t = d0[0];

            if (t == 0) return -1;
            if (t < 0x80u) {
                off += 32;
                continue;
            }
            if (t != 0x85) {
                off += 32;
                continue;
            }

            uint8_t sec_count = d0[1];
            uint16_t attr = rd16(d0 + 4);
            if (attr & 0x0010) {
                off += (uint32_t)(1 + sec_count) * 32;
                continue;
            }

            if (off + (uint32_t)(1 + sec_count) * 32 > cluster_bytes) return -1;

            uint8_t ds[32];
            copy_dentry_at(lba0, off + 32, ds);
            if (ds[0] != 0xC0) {
                off += 32;
                continue;
            }

            uint8_t name_len = ds[3];
            uint32_t first_clu = rd32(ds + 20);
            uint64_t fsize = rd64(ds + 24);

            if (name_len == 0) {
                off += (uint32_t)(1 + sec_count) * 32;
                continue;
            }

            uint16_t wname[256];
            int ni = 0;
            int name_entries = (name_len + 14) / 15;
            bool bad = false;
            for (int ne = 0; ne < name_entries && !bad; ne++) {
                uint8_t dn[32];
                copy_dentry_at(lba0, off + (uint32_t)(2 + ne) * 32, dn);
                if (dn[0] != 0xC1) {
                    bad = true;
                    break;
                }
                int in_this = (ne == name_entries - 1) ? (name_len - ne * 15) : 15;
                for (int j = 0; j < in_this; j++) {
                    if (ni >= 255) {
                        bad = true;
                        break;
                    }
                    wname[ni++] = rd16(dn + 2 + j * 2);
                }
            }
            if (bad || ni != name_len) {
                off += (uint32_t)(1 + sec_count) * 32;
                continue;
            }
            wname[ni] = 0;

            if (name_matches_expected(expected, wname, name_len)) {
                *out_cluster = first_clu;
                *out_size = (uint32_t)fsize;
                exfat_nofatchain = (ds[1] & 0x02) != 0;
                return 0;
            }

            off += (uint32_t)(1 + sec_count) * 32;
        }

        dir_clu = get_fat_entry_exfat(dir_clu);
    }
    return -1;
}

int fat_find_root(const char *name_8_3, uint32_t *out_cluster, uint32_t *out_size)
{
    if (fs_kind == FS_EXFAT) return exfat_find_root(name_8_3, out_cluster, out_size);

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

    if (fs_kind == FS_EXFAT) {
        while (read < size && cluster >= 2 && cluster < 0xFFFFFFF8u) {
            uint32_t lba = exfat_cluster_to_lba(cluster);
            for (uint8_t s = 0; s < sectors_per_cluster && read < size; s++) {
                if (ata_read_sectors(lba + s, 1, sector_buf) != 0) return (int)read;
                uint32_t chunk = bytes_per_sector;
                if (read + chunk > size) chunk = size - read;
                for (uint32_t i = 0; i < chunk; i++) p[read + i] = sector_buf[i];
                read += chunk;
            }
            if (exfat_nofatchain) {
                cluster++;
            } else {
                cluster = get_fat_entry_exfat(cluster);
            }
        }
        return (int)read;
    }

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
        cluster = get_fat_entry_fat1216(cluster);
    }
    return (int)read;
}
