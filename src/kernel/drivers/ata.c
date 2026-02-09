/**
 * ATA PIO driver - primary master, LBA28.
 * Ports: 0x1F0-0x1F7 (data, error, count, LBA low/mid/hi, drive, command).
 */

#include <kernel/ata.h>
#include <kernel/port.h>
#include <kernel/types.h>

#define ATA_DATA    0x1F0
#define ATA_ERROR   0x1F1
#define ATA_COUNT   0x1F2
#define ATA_LBA0    0x1F3
#define ATA_LBA1    0x1F4
#define ATA_LBA2    0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7
#define ATA_ALT     0x3F6

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30
#define ATA_DRIVE_LBA 0xE0
#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_ERR 0x01

static void wait_bsy(void)
{
    while (inb(ATA_CMD) & ATA_STATUS_BSY)
        ;
}

static void wait_drq(void)
{
    while (!(inb(ATA_CMD) & ATA_STATUS_DRQ))
        ;
}

int ata_read_sectors(uint32_t lba, uint32_t count, void *buf)
{
    if (count == 0) return 0;
    wait_bsy();
    outb(ATA_DRIVE, ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT, (uint8_t)count);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, ATA_CMD_READ);
    uint16_t *p = (uint16_t *)buf;
    for (uint32_t s = 0; s < count; s++) {
        wait_bsy();
        wait_drq();
        if (inb(ATA_CMD) & ATA_STATUS_ERR) return -1;
        for (unsigned i = 0; i < ATA_SECTOR_SIZE / 2; i++)
            p[i] = inw(ATA_DATA);
        p += ATA_SECTOR_SIZE / 2;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void *buf)
{
    if (count == 0) return 0;
    wait_bsy();
    outb(ATA_DRIVE, ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
    outb(ATA_COUNT, (uint8_t)count);
    outb(ATA_LBA0, (uint8_t)(lba));
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, ATA_CMD_WRITE);
    const uint16_t *p = (const uint16_t *)buf;
    for (uint32_t s = 0; s < count; s++) {
        wait_bsy();
        wait_drq();
        if (inb(ATA_CMD) & ATA_STATUS_ERR) return -1;
        for (unsigned i = 0; i < ATA_SECTOR_SIZE / 2; i++)
            outw(ATA_DATA, p[i]);
        p += ATA_SECTOR_SIZE / 2;
    }
    wait_bsy();
    return 0;
}
