/**
 * FAT12/16 and exFAT — mount, find in root, read/write files in root directory.
 */

#include <kernel/fat.h>
#include <kernel/ata.h>
#include <kernel/types.h>

enum { FS_NONE, FS_FAT1216, FS_EXFAT };

enum { CS_DIR_ENTRY = 0, CS_DEFAULT = 2 };

static uint8_t fs_kind;
static uint32_t fat_start_lba;
static uint32_t fat_sectors;
static uint32_t root_start_lba;
static uint32_t root_sectors;
static uint32_t data_start_lba;
static uint8_t sectors_per_cluster;
static uint16_t bytes_per_sector;
static bool is_fat12;
static uint32_t fat_total_clusters;
static uint8_t fat_num_fats;

/* exFAT */
static uint32_t exfat_root_cluster;
static uint32_t exfat_cluster_count;
static uint8_t exfat_num_fats;
static uint32_t exfat_bitmap_clu;
static uint64_t exfat_bitmap_bytes;
static bool exfat_bitmap_valid;
/* Set by fat_find_root on exFAT when stream entry has NoFatChain (contiguous clusters). */
static bool exfat_nofatchain;

static uint8_t sector_buf[512];
static uint8_t fat_sector_buf[512];
static uint8_t fat12_2sect[1024];

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

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void wr64(uint8_t *p, uint64_t v)
{
    wr32(p, (uint32_t)v);
    wr32(p + 4, (uint32_t)(v >> 32));
}

static void mem_set(uint8_t *p, uint8_t v, size_t n)
{
    for (size_t i = 0; i < n; i++) p[i] = v;
}

static int exfat_bitmap_is_free(uint32_t cluster, bool *out);
static int exfat_bitmap_modify_bit(uint32_t cluster, bool set_bit);

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

static int write_bytes_at_lba(uint32_t first_lba, uint32_t off, const uint8_t *src, uint32_t len)
{
    uint32_t sec = off / bytes_per_sector;
    uint32_t rel = off % bytes_per_sector;
    if (rel + len <= bytes_per_sector) {
        if (ata_read_sectors(first_lba + sec, 1, sector_buf) != 0) return -1;
        for (uint32_t i = 0; i < len; i++) sector_buf[rel + i] = src[i];
        return ata_write_sectors(first_lba + sec, 1, sector_buf);
    }
    uint32_t n0 = bytes_per_sector - rel;
    if (ata_read_sectors(first_lba + sec, 1, sector_buf) != 0) return -1;
    for (uint32_t i = 0; i < n0; i++) sector_buf[rel + i] = src[i];
    if (ata_write_sectors(first_lba + sec, 1, sector_buf) != 0) return -1;
    if (ata_read_sectors(first_lba + sec + 1, 1, sector_buf) != 0) return -1;
    for (uint32_t i = n0; i < len; i++) sector_buf[i - n0] = src[i];
    return ata_write_sectors(first_lba + sec + 1, 1, sector_buf);
}

static int write_dentry_at(uint32_t cluster_first_lba, uint32_t off, const uint8_t d[32])
{
    return write_bytes_at_lba(cluster_first_lba, off, d, 32);
}

static int sync_fat_sector_index(uint32_t idx_within_fat)
{
    if (idx_within_fat >= fat_sectors) return -1;
    for (uint32_t c = 0; c < (uint32_t)fat_num_fats; c++) {
        uint32_t lba = fat_start_lba + c * fat_sectors + idx_within_fat;
        if (ata_write_sectors(lba, 1, fat_sector_buf) != 0) return -1;
    }
    return 0;
}

static int fat16_set_entry(uint32_t cluster, uint16_t val)
{
    uint32_t offset = cluster * 2;
    uint32_t sec = offset / bytes_per_sector;
    uint32_t off = offset % bytes_per_sector;
    if (ata_read_sectors(fat_start_lba + sec, 1, fat_sector_buf) != 0) return -1;
    wr16(fat_sector_buf + off, val);
    return sync_fat_sector_index(sec);
}

static int fat12_set_entry(uint32_t cluster, uint16_t val)
{
    val &= 0x0FFFu;
    uint32_t o = cluster * 3 / 2;
    uint32_t sec0 = o / bytes_per_sector;
    uint32_t rel = o % bytes_per_sector;
    int span = (rel + 1 >= bytes_per_sector) ? 1 : 0;

    if (ata_read_sectors(fat_start_lba + sec0, 1, fat12_2sect) != 0) return -1;
    if (span) {
        if (ata_read_sectors(fat_start_lba + sec0 + 1, 1, fat12_2sect + bytes_per_sector) != 0)
            return -1;
    }
    uint8_t *b = fat12_2sect + rel;
    if ((cluster & 1) == 0) {
        b[0] = (uint8_t)(val & 0xFF);
        b[1] = (uint8_t)((b[1] & 0xF0) | ((val >> 8) & 0x0F));
    } else {
        b[0] = (uint8_t)((b[0] & 0x0F) | ((val << 4) & 0xF0));
        b[1] = (uint8_t)((val >> 4) & 0xFF);
    }
    if (ata_write_sectors(fat_start_lba + sec0, 1, fat12_2sect) != 0) return -1;
    if (span) {
        if (ata_write_sectors(fat_start_lba + sec0 + 1, 1, fat12_2sect + bytes_per_sector) != 0)
            return -1;
    }
    for (uint32_t c = 1; c < (uint32_t)fat_num_fats; c++) {
        uint32_t base = fat_start_lba + c * fat_sectors;
        if (ata_write_sectors(base + sec0, 1, fat12_2sect) != 0) return -1;
        if (span) {
            if (ata_write_sectors(base + sec0 + 1, 1, fat12_2sect + bytes_per_sector) != 0)
                return -1;
        }
    }
    return 0;
}

static int fat_set_entry_fat1216(uint32_t cluster, uint16_t val)
{
    if (is_fat12) return fat12_set_entry(cluster, val);
    return fat16_set_entry(cluster, val);
}

static int exfat_set_fat_entry(uint32_t cluster, uint32_t val)
{
    uint32_t offset = cluster * 4;
    uint32_t sec = offset / bytes_per_sector;
    uint32_t off = offset % bytes_per_sector;
    for (uint32_t fc = 0; fc < (uint32_t)exfat_num_fats; fc++) {
        uint32_t base = fat_start_lba + fc * fat_sectors + sec;
        if (off + 4 <= bytes_per_sector) {
            if (ata_read_sectors(base, 1, fat_sector_buf) != 0) return -1;
            wr32(fat_sector_buf + off, val);
            if (ata_write_sectors(base, 1, fat_sector_buf) != 0) return -1;
        } else {
            uint8_t tmp[4];
            wr32(tmp, val);
            uint32_t n0 = bytes_per_sector - off;
            if (ata_read_sectors(base, 1, fat_sector_buf) != 0) return -1;
            if (ata_read_sectors(base + 1, 1, sector_buf) != 0) return -1;
            for (uint32_t i = 0; i < n0; i++) fat_sector_buf[off + i] = tmp[i];
            for (uint32_t i = n0; i < 4; i++) sector_buf[i - n0] = tmp[i];
            if (ata_write_sectors(base, 1, fat_sector_buf) != 0) return -1;
            if (ata_write_sectors(base + 1, 1, sector_buf) != 0) return -1;
        }
    }
    return 0;
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

static int exfat_locate_bitmap(void)
{
    uint32_t dir_clu = exfat_root_cluster;
    uint32_t cb = (uint32_t)sectors_per_cluster * bytes_per_sector;
    while (dir_clu >= 2 && dir_clu < 0xFFFFFFF8u) {
        uint32_t lba0 = exfat_cluster_to_lba(dir_clu);
        for (uint32_t off = 0; off + 32 <= cb; off += 32) {
            uint8_t d[32];
            copy_dentry_at(lba0, off, d);
            if (d[0] == 0) break;
            if (d[0] == 0x81) {
                exfat_bitmap_clu = rd32(d + 20);
                exfat_bitmap_bytes = rd64(d + 24);
                exfat_bitmap_valid = (exfat_bitmap_clu >= 2 && exfat_bitmap_bytes > 0);
                return exfat_bitmap_valid ? 0 : -1;
            }
        }
        dir_clu = get_fat_entry_exfat(dir_clu);
    }
    return -1;
}

static int exfat_mount(const uint8_t *boot)
{
    exfat_nofatchain = false;
    exfat_bitmap_valid = false;
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
    exfat_cluster_count = rd32(boot + 0x5C);
    exfat_root_cluster = rd32(boot + 0x60);
    exfat_num_fats = boot[0x6E];
    if (exfat_num_fats == 0) exfat_num_fats = 1;

    if (exfat_root_cluster < 2) return -1;
    if (fat_sectors == 0 || data_start_lba == 0) return -1;

    fs_kind = FS_EXFAT;
    (void)exfat_locate_bitmap();
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
    fat_num_fats = p->num_fats;
    if (fat_num_fats == 0) fat_num_fats = 1;
    fat_start_lba = p->reserved_sectors;
    root_start_lba = fat_start_lba + p->num_fats * fat_sectors;
    data_start_lba = root_start_lba + root_sectors;
    uint32_t total = p->total_sectors_16 ? p->total_sectors_16 : p->total_sectors_32;
    uint32_t data_sectors = total - (p->reserved_sectors + p->num_fats * fat_sectors + root_sectors);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    fat_total_clusters = total_clusters;
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

struct exfat_loc {
    uint32_t dir_clu;
    uint32_t off;
    uint8_t sec_count;
};

static int exfat_find_root(const char *name_83, uint32_t *out_cluster, uint32_t *out_size,
                           struct exfat_loc *loc_opt)
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
                if (loc_opt) {
                    loc_opt->dir_clu = dir_clu;
                    loc_opt->off = off;
                    loc_opt->sec_count = sec_count;
                }
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
    if (fs_kind == FS_EXFAT) return exfat_find_root(name_8_3, out_cluster, out_size, NULL);

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

static void normalize_83(char out[11], const char *in)
{
    for (int i = 0; i < 11; i++) out[i] = ascii_upper(in[i]);
}

static uint32_t fat1216_find_free_cluster(void)
{
    uint16_t bad = is_fat12 ? 0x0FF7u : 0xFFF7u;
    for (uint32_t c = 2; c < 2 + fat_total_clusters; c++) {
        uint16_t e = (uint16_t)get_fat_entry_fat1216(c);
        if (e == bad) continue;
        if (e == 0) return c;
    }
    return 0;
}

static void fat1216_free_chain(uint32_t start)
{
    uint32_t c = start;
    uint16_t eoc = is_fat12 ? 0x0FF8u : 0xFFF8u;
    while (c >= 2 && c < (uint32_t)eoc) {
        uint32_t next = get_fat_entry_fat1216(c);
        fat_set_entry_fat1216(c, 0);
        c = next;
    }
}

static int fat1216_alloc_chain(uint32_t n, uint32_t *out_first)
{
    if (n == 0) {
        *out_first = 0;
        return 0;
    }
    uint16_t eoc = is_fat12 ? 0x0FF8u : 0xFFF8u;
    uint32_t prev = 0, first = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = fat1216_find_free_cluster();
        if (c == 0) {
            if (i > 0) fat1216_free_chain(first);
            return -1;
        }
        if (i > 0) fat_set_entry_fat1216(prev, (uint16_t)c);
        else first = c;
        fat_set_entry_fat1216(c, eoc);
        prev = c;
    }
    *out_first = first;
    return 0;
}

static int fat1216_write_bytes(uint32_t first_clu, const void *buf, uint32_t size)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t cluster = first_clu;
    uint32_t written = 0;
    uint16_t eoc = is_fat12 ? 0x0FF8u : 0xFFF8u;
    while (written < size && cluster >= 2 && cluster < (uint32_t)eoc) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster && written < size; s++) {
            uint32_t chunk = bytes_per_sector;
            if (written + chunk > size) chunk = size - written;
            for (uint32_t i = 0; i < chunk; i++) sector_buf[i] = p[written + i];
            for (uint32_t i = chunk; i < bytes_per_sector; i++) sector_buf[i] = 0;
            if (ata_write_sectors(lba + s, 1, sector_buf) != 0) return -1;
            written += chunk;
        }
        cluster = get_fat_entry_fat1216(cluster);
    }
    return written == size ? 0 : -1;
}

static int fat1216_find_slot(const char name11[11], int *found_match, uint32_t *out_lba, uint32_t *out_idx)
{
    uint32_t entries_per_sector = bytes_per_sector / FAT_ROOT_ENTRY_SIZE;
    uint32_t best_e5_lba = 0, best_e5_i = 0;
    bool has_e5 = false;
    bool has_end = false;
    uint32_t end_lba = 0, end_i = 0;

    for (uint32_t s = 0; s < root_sectors; s++) {
        uint32_t lba = root_start_lba + s;
        if (ata_read_sectors(lba, 1, sector_buf) != 0) return -1;
        struct fat_dir_entry *e = (struct fat_dir_entry *)sector_buf;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (e[i].name[0] == 0x00) {
                end_lba = lba;
                end_i = i;
                has_end = true;
                s = root_sectors;
                break;
            }
            if (e[i].name[0] == 0xE5 && !has_e5) {
                has_e5 = true;
                best_e5_lba = lba;
                best_e5_i = i;
            }
            size_t k = 0;
            for (; k < 11; k++)
                if ((uint8_t)e[i].name[k] != (uint8_t)name11[k]) break;
            if (k == 11) {
                *found_match = 1;
                *out_lba = lba;
                *out_idx = i;
                return 0;
            }
        }
    }
    *found_match = 0;
    if (has_e5) {
        *out_lba = best_e5_lba;
        *out_idx = best_e5_i;
        return 0;
    }
    if (has_end) {
        *out_lba = end_lba;
        *out_idx = end_i;
        return 0;
    }
    return -1;
}

static int fat1216_write_root(const char *name_83, const void *buf, uint32_t size)
{
    char up[11];
    normalize_83(up, name_83);

    int found = 0;
    uint32_t ep_lba = 0, ep_idx = 0;
    if (fat1216_find_slot(up, &found, &ep_lba, &ep_idx) != 0) return -1;

    uint32_t old_c = 0, old_sz = 0;
    if (found && fat_find_root(name_83, &old_c, &old_sz) == 0) fat1216_free_chain(old_c);

    uint32_t cbytes = (uint32_t)sectors_per_cluster * bytes_per_sector;
    uint32_t ncl = 0;
    if (size > 0) ncl = (size + cbytes - 1) / cbytes;
    uint32_t first = 0;
    if (fat1216_alloc_chain(ncl, &first) != 0) return -1;
    if (size > 0 && fat1216_write_bytes(first, buf, size) != 0) {
        fat1216_free_chain(first);
        return -1;
    }

    if (ata_read_sectors(ep_lba, 1, sector_buf) != 0) return -1;
    struct fat_dir_entry *slot = (struct fat_dir_entry *)(sector_buf + ep_idx * 32);
    mem_set((uint8_t *)slot, 0, sizeof(*slot));
    for (int i = 0; i < 11; i++) slot->name[i] = up[i];
    slot->attr = FAT_ATTR_ARCHIVE;
    slot->size = size;
    slot->first_cluster_lo = (uint16_t)(first & 0xFFFFu);
    slot->first_cluster_hi = (uint16_t)((first >> 16) & 0xFFFFu);
    if (ata_write_sectors(ep_lba, 1, sector_buf) != 0) return -1;

    if (!found && ep_idx + 1 < bytes_per_sector / FAT_ROOT_ENTRY_SIZE) {
        if (ata_read_sectors(ep_lba, 1, sector_buf) != 0) return -1;
        struct fat_dir_entry *nx = (struct fat_dir_entry *)(sector_buf + (ep_idx + 1) * 32);
        if (nx->name[0] != 0x00) {
            mem_set((uint8_t *)nx, 0, sizeof(*nx));
            if (ata_write_sectors(ep_lba, 1, sector_buf) != 0) return -1;
        }
    }
    return 0;
}

static uint16_t exfat_chksum16_buf(const uint8_t *data, int len, uint16_t sum, int typ)
{
    for (int i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (typ == CS_DIR_ENTRY && (i == 2 || i == 3)) continue;
        sum = (uint16_t)(((sum << 15) | (sum >> 1)) + b);
    }
    return sum;
}

static void exfat_name_utf16_from_83(const char *name_83, uint16_t *wout, int *out_len)
{
    char exp[64];
    build_expected_from_83(name_83, exp, sizeof(exp));
    int n = 0;
    for (int i = 0; exp[i] && n < 255; i++) wout[n++] = (uint16_t)(uint8_t)exp[i];
    *out_len = n;
}

static uint32_t exfat_grab_cluster(void)
{
    for (uint32_t c = 2; c < 2 + exfat_cluster_count; c++) {
        bool fr = false;
        if (!exfat_bitmap_valid) return 0;
        if (exfat_bitmap_is_free(c, &fr) != 0) return 0;
        if (fr) {
            if (exfat_bitmap_modify_bit(c, true) != 0) return 0;
            return c;
        }
    }
    return 0;
}

static int exfat_bitmap_is_free(uint32_t cluster, bool *out)
{
    if (!exfat_bitmap_valid || cluster < 2) return -1;
    uint64_t bit = (uint64_t)cluster - 2;
    if (bit / 8 >= exfat_bitmap_bytes) return -1;
    uint64_t byte_off = bit / 8;
    uint32_t bit_in_byte = (uint32_t)(bit % 8);
    uint32_t lba = exfat_cluster_to_lba(exfat_bitmap_clu) + (uint32_t)(byte_off / bytes_per_sector);
    uint32_t rel = (uint32_t)(byte_off % bytes_per_sector);
    if (ata_read_sectors(lba, 1, sector_buf) != 0) return -1;
    *out = ((sector_buf[rel] >> bit_in_byte) & 1) == 0;
    return 0;
}

static int exfat_bitmap_modify_bit(uint32_t cluster, bool set_bit)
{
    if (!exfat_bitmap_valid || cluster < 2) return -1;
    uint64_t bit = (uint64_t)cluster - 2;
    if (bit / 8 >= exfat_bitmap_bytes) return -1;
    uint64_t byte_off = bit / 8;
    uint32_t bit_in_byte = (uint32_t)(bit % 8);
    uint32_t lba = exfat_cluster_to_lba(exfat_bitmap_clu) + (uint32_t)(byte_off / bytes_per_sector);
    uint32_t rel = (uint32_t)(byte_off % bytes_per_sector);
    if (ata_read_sectors(lba, 1, sector_buf) != 0) return -1;
    if (set_bit)
        sector_buf[rel] |= (uint8_t)(1u << bit_in_byte);
    else
        sector_buf[rel] &= (uint8_t)(~(1u << bit_in_byte));
    return ata_write_sectors(lba, 1, sector_buf);
}

static void exfat_release_cluster(uint32_t c)
{
    if (c < 2) return;
    exfat_set_fat_entry(c, 0);
    (void)exfat_bitmap_modify_bit(c, false);
}

static void exfat_free_chain(uint32_t start)
{
    uint32_t c = start;
    while (c >= 2 && c < 0xFFFFFFF8u) {
        uint32_t next = get_fat_entry_exfat(c);
        exfat_release_cluster(c);
        c = next;
    }
}

static int exfat_alloc_chain(uint32_t n, uint32_t *out_first)
{
    if (n == 0) {
        *out_first = 0;
        return 0;
    }
    uint32_t prev = 0, first = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = exfat_grab_cluster();
        if (c == 0) {
            if (i > 0) exfat_free_chain(first);
            return -1;
        }
        if (i > 0) exfat_set_fat_entry(prev, c);
        else first = c;
        exfat_set_fat_entry(c, 0xFFFFFFFFu);
        prev = c;
    }
    *out_first = first;
    return 0;
}

static int exfat_write_chain(uint32_t first_clu, const void *buf, uint32_t size)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t cluster = first_clu;
    uint32_t written = 0;
    while (written < size && cluster >= 2 && cluster < 0xFFFFFFF8u) {
        uint32_t lba = exfat_cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster && written < size; s++) {
            uint32_t chunk = bytes_per_sector;
            if (written + chunk > size) chunk = size - written;
            for (uint32_t i = 0; i < chunk; i++) sector_buf[i] = p[written + i];
            for (uint32_t i = chunk; i < bytes_per_sector; i++) sector_buf[i] = 0;
            if (ata_write_sectors(lba + s, 1, sector_buf) != 0) return -1;
            written += chunk;
        }
        cluster = get_fat_entry_exfat(cluster);
    }
    return written == size ? 0 : -1;
}

static int exfat_find_insert(uint32_t need_bytes, uint32_t *dclu, uint32_t *doff)
{
    uint32_t dir_clu = exfat_root_cluster;
    uint32_t cb = (uint32_t)sectors_per_cluster * bytes_per_sector;
    while (dir_clu >= 2 && dir_clu < 0xFFFFFFF8u) {
        uint32_t lba0 = exfat_cluster_to_lba(dir_clu);
        for (uint32_t off = 0; off + 32 <= cb; off += 32) {
            uint8_t d[32];
            copy_dentry_at(lba0, off, d);
            if (d[0] == 0) {
                if (off + need_bytes + 32 <= cb) {
                    *dclu = dir_clu;
                    *doff = off;
                    return 0;
                }
                break;
            }
        }
        dir_clu = get_fat_entry_exfat(dir_clu);
    }
    return -1;
}

static int exfat_write_root(const char *name_83, const void *buf, uint32_t size)
{
    if (!exfat_bitmap_valid) return -1;

    uint16_t wname[256];
    int name_len = 0;
    exfat_name_utf16_from_83(name_83, wname, &name_len);
    if (name_len == 0 || name_len > 255) return -1;

    int name_ent = (name_len + 14) / 15;
    uint8_t sec_count = (uint8_t)(1 + name_ent);
    uint32_t need = (uint32_t)(1 + sec_count) * 32;

    uint32_t old_c = 0, old_sz = 0;
    struct exfat_loc loc;
    int existed = (exfat_find_root(name_83, &old_c, &old_sz, &loc) == 0);
    if (existed) exfat_free_chain(old_c);

    uint32_t cbytes = (uint32_t)sectors_per_cluster * bytes_per_sector;
    uint32_t ncl = 0;
    if (size > 0) ncl = (size + cbytes - 1) / cbytes;
    uint32_t first = 0;
    if (exfat_alloc_chain(ncl, &first) != 0) return -1;
    if (size > 0 && exfat_write_chain(first, buf, size) != 0) {
        exfat_free_chain(first);
        return -1;
    }

    uint16_t name_hash = exfat_chksum16_buf((const uint8_t *)wname, name_len * 2, 0, CS_DEFAULT);

    uint8_t set[32 * 24];
    int nent = 1 + sec_count;
    if (nent > 24) {
        exfat_free_chain(first);
        return -1;
    }
    mem_set(set, 0, sizeof(set));

    set[0] = 0x85;
    set[1] = sec_count;
    wr16(set + 4, 0x0020);

    set[32] = 0xC0;
    set[33] = 0x01;
    set[35] = (uint8_t)name_len;
    wr16(set + 36, name_hash);
    wr64(set + 40, (uint64_t)size);
    wr32(set + 48, 0);
    wr32(set + 52, first);
    wr64(set + 56, (uint64_t)size);

    for (int ni = 0; ni < name_ent; ni++) {
        uint8_t *ne = set + (2 + ni) * 32;
        ne[0] = 0xC1;
        ne[1] = 0;
        int in_this = (ni == name_ent - 1) ? (name_len - ni * 15) : 15;
        for (int j = 0; j < in_this; j++) wr16(ne + 2 + j * 2, wname[ni * 15 + j]);
    }

    uint16_t dcs = exfat_chksum16_buf(set, 32, 0, CS_DIR_ENTRY);
    for (int i = 1; i < nent; i++) dcs = exfat_chksum16_buf(set + i * 32, 32, dcs, CS_DEFAULT);
    wr16(set + 2, dcs);

    uint32_t wclu = 0, woff = 0;
    if (existed) {
        if (sec_count != loc.sec_count) {
            exfat_free_chain(first);
            return -1;
        }
        wclu = loc.dir_clu;
        woff = loc.off;
    } else {
        if (exfat_find_insert(need, &wclu, &woff) != 0) {
            exfat_free_chain(first);
            return -1;
        }
    }

    uint32_t lba0 = exfat_cluster_to_lba(wclu);
    for (int i = 0; i < nent; i++) {
        if (write_dentry_at(lba0, woff + (uint32_t)i * 32, set + i * 32) != 0) {
            exfat_free_chain(first);
            return -1;
        }
    }
    if (!existed) {
        uint8_t z[32];
        mem_set(z, 0, 32);
        if (write_dentry_at(lba0, woff + need, z) != 0) {
            exfat_free_chain(first);
            return -1;
        }
    }

    exfat_nofatchain = false;
    return 0;
}

int fat_write_root(const char *name_8_3, const void *buf, uint32_t size)
{
    if (fs_kind == FS_NONE) return -1;
    if (fs_kind == FS_EXFAT) return exfat_write_root(name_8_3, buf, size);
    return fat1216_write_root(name_8_3, buf, size);
}
