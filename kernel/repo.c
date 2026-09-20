#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

extern bool ata_read_sectors(uint32_t lba, uint8_t count, void *buf);

#ifndef REPO_START_LBA
#define REPO_START_LBA 2000
#endif

#define SECTOR 512
#define MAX_INDEX_ENTRIES 12
#define MAX_PACKAGE_SECTORS 64

typedef struct {
    char name[32];
    uint32_t lba;
    uint32_t sectors;
} RepoEntry;

bool repo_find(const char *name, RepoEntry *out) {
    uint8_t sector[SECTOR];
    if (!ata_read_sectors(REPO_START_LBA, 1, sector)) return false;
    if (memcmp(sector, "FLOPREPO", 8) != 0) return false;

    uint32_t count;
    memcpy(&count, sector + 8, 4);
    if (count > MAX_INDEX_ENTRIES) count = MAX_INDEX_ENTRIES;

    const uint8_t *p = sector + 12;
    for (uint32_t i = 0; i < count; i++) {
        char entry_name[33];
        memcpy(entry_name, p, 32);
        entry_name[32] = '\0';
        uint32_t lba, sectors;
        memcpy(&lba, p + 32, 4);
        memcpy(&sectors, p + 36, 4);
        if (strcmp(entry_name, name) == 0) {
            memcpy(out->name, entry_name, sizeof(out->name));
            out->lba = REPO_START_LBA + lba;
            out->sectors = sectors;
            return true;
        }
        p += 40;
    }
    return false;
}

int repo_fetch(const RepoEntry *entry, char *glyph_out, char *content_out, int content_out_size) {
    if (entry->sectors == 0 || entry->sectors > MAX_PACKAGE_SECTORS) return -1;
    static uint8_t buf[MAX_PACKAGE_SECTORS * SECTOR];
    if (!ata_read_sectors(entry->lba, (uint8_t)entry->sectors, buf)) return -1;
    if (memcmp(buf, "FLOP", 4) != 0) return -1;

    if (glyph_out) memcpy(glyph_out, buf + 36, 4);
    uint32_t content_length;
    memcpy(&content_length, buf + 44, 4);

    int n = (int)content_length;
    if (n > content_out_size - 1) n = content_out_size - 1;
    if (n < 0) n = 0;
    memcpy(content_out, buf + 48, (size_t)n);
    content_out[n] = '\0';
    return n;
}
