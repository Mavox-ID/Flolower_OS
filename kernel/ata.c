#include <stdint.h>
#include <stdbool.h>
#include "io.h"

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_CMD_READ_SECTORS  0x20

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static void ata_wait_bsy_clear(void) {
    while (inb(ATA_STATUS) & ATA_SR_BSY) { }
}

static bool ata_wait_drq(void) {
    for (;;) {
        uint8_t st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR) return false;
        if (st & ATA_SR_DRQ) return true;
    }
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void *buf) {
    if (count == 0) return false;
    uint16_t *dst = (uint16_t *)buf;

    ata_wait_bsy_clear();
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    for (int s = 0; s < count; s++) {
        if (!ata_wait_drq()) return false;
        for (int i = 0; i < 256; i++) *dst++ = inw(ATA_DATA);
    }
    return true;
}
