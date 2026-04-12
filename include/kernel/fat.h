#ifndef BONFIRE_FAT_H
#define BONFIRE_FAT_H

#include <kernel/types.h>

#define FAT_NAME_MAX 11
#define FAT_ROOT_ENTRY_SIZE 32

struct fat_bpb {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed));

struct fat_dir_entry {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster_hi;
    uint16_t time_creat;
    uint16_t date_creat;
    uint16_t first_cluster_lo;
    uint32_t size;
} __attribute__((packed));

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIR       0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ENTRY_FREE     0x00
#define FAT_ENTRY_EOC12    0x0FF8
#define FAT_ENTRY_EOC16    0xFFF8
#define FAT_ENTRY_BAD      0x0FF7

/* Mount disk (read BPB / exFAT VBR from LBA 0). Tries FAT12/16 then exFAT. Returns 0 on success. */
int fat_mount(void);
/* Find file in root by name (e.g. "FILE    TXT"). FAT: 8.3 match; exFAT: same tokenization vs UTF-16 name). */
int fat_find_root(const char *name_8_3, uint32_t *out_cluster, uint32_t *out_size);
/* Read file content: start at cluster, follow FAT chain, fill buf (max size bytes). Returns bytes read. */
int fat_read_file(uint32_t start_cluster, uint32_t size, void *buf);
/* Create or replace file in root (8.3 name). Writes size bytes from buf. Returns 0 on success. */
int fat_write_root(const char *name_8_3, const void *buf, uint32_t size);

#endif /* BONFIRE_FAT_H */
