#ifndef BONFIRE_ATA_H
#define BONFIRE_ATA_H

#include <kernel/types.h>

#define ATA_SECTOR_SIZE 512

/* Read one or more sectors (LBA28, primary master). Returns 0 on success. */
int ata_read_sectors(uint32_t lba, uint32_t count, void *buf);
/* Write sectors. Returns 0 on success. */
int ata_write_sectors(uint32_t lba, uint32_t count, const void *buf);

#endif /* BONFIRE_ATA_H */
