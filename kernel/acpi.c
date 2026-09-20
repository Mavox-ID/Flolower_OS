#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "acpi.h"

extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int   memcmp(const void *a, const void *b, size_t n);

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) AcpiSdtHeader;

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) AcpiRsdp;

static uint8_t sum_bytes(const void *p, uint32_t len) {
    const uint8_t *b = (const uint8_t *)p;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum = (uint8_t)(sum + b[i]);
    return sum;
}

static bool sig_is(const AcpiSdtHeader *h, const char *sig4) {
    return memcmp(h->signature, sig4, 4) == 0;
}

static const AcpiRsdp *find_rsdp(void) {
    uint16_t ebda_seg = *(const uint16_t *)(uintptr_t)0x40E;
    uint32_t ebda_addr = (uint32_t)ebda_seg << 4;

    if (ebda_addr != 0) {
        for (uint32_t addr = ebda_addr; addr < ebda_addr + 1024; addr += 16) {
            const AcpiRsdp *r = (const AcpiRsdp *)(uintptr_t)addr;
            if (memcmp(r->signature, "RSD PTR ", 8) == 0 && sum_bytes(r, 20) == 0) {
                return r;
            }
        }
    }

    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const AcpiRsdp *r = (const AcpiRsdp *)(uintptr_t)addr;
        if (memcmp(r->signature, "RSD PTR ", 8) == 0 && sum_bytes(r, 20) == 0) {
            return r;
        }
    }
    return NULL;
}

static bool sdt_checksum_ok(const AcpiSdtHeader *h) {
    if (h->length < sizeof(AcpiSdtHeader) || h->length > (16u * 1024 * 1024)) return false;
    return sum_bytes(h, h->length) == 0;
}

static void add_blob(AcpiInfo *out, const AcpiSdtHeader *h) {
    if (out->n_blobs >= ACPI_MAX_BLOBS) return;
    if (!sdt_checksum_ok(h)) return;
    const uint8_t *aml = (const uint8_t *)h + sizeof(AcpiSdtHeader);
    uint32_t len = h->length - sizeof(AcpiSdtHeader);
    out->blobs[out->n_blobs].aml = aml;
    out->blobs[out->n_blobs].len = len;
    out->n_blobs++;
}

void acpi_discover(AcpiInfo *out) {
    memset(out, 0, sizeof(*out));

    const AcpiRsdp *rsdp = find_rsdp();
    if (!rsdp) return;

    bool have_xsdt = false;
    const AcpiSdtHeader *root = NULL;

    if (rsdp->revision >= 2 && rsdp->length >= sizeof(AcpiRsdp) &&
        sum_bytes(rsdp, rsdp->length) == 0 &&
        rsdp->xsdt_address != 0 && (rsdp->xsdt_address >> 32) == 0) {
        const AcpiSdtHeader *xsdt = (const AcpiSdtHeader *)(uintptr_t)(uint32_t)rsdp->xsdt_address;
        if (sig_is(xsdt, "XSDT") && sdt_checksum_ok(xsdt)) {
            root = xsdt;
            have_xsdt = true;
        }
    }
    if (!root && rsdp->rsdt_address != 0) {
        const AcpiSdtHeader *rsdt = (const AcpiSdtHeader *)(uintptr_t)rsdp->rsdt_address;
        if (sig_is(rsdt, "RSDT") && sdt_checksum_ok(rsdt)) root = rsdt;
    }
    if (!root) return;

    const uint8_t *entries = (const uint8_t *)root + sizeof(AcpiSdtHeader);
    uint32_t entries_bytes = root->length - sizeof(AcpiSdtHeader);
    uint32_t entry_size = have_xsdt ? 8 : 4;
    uint32_t n_entries = entries_bytes / entry_size;

    const AcpiSdtHeader *fadt = NULL;

    for (uint32_t i = 0; i < n_entries; i++) {
        uint32_t phys;
        if (have_xsdt) {
            uint64_t v;
            memcpy(&v, entries + i * 8, 8);
            if ((v >> 32) != 0) continue;
            phys = (uint32_t)v;
        } else {
            memcpy(&phys, entries + i * 4, 4);
        }
        if (phys == 0) continue;

        const AcpiSdtHeader *h = (const AcpiSdtHeader *)(uintptr_t)phys;
        if (!sdt_checksum_ok(h)) continue;

        if (sig_is(h, "FACP")) {
            fadt = h;
        } else if (sig_is(h, "SSDT")) {
            add_blob(out, h);
        }
    }

    if (fadt) {
        if (fadt->length >= 68) {
            uint32_t pm1a = 0, pm1b = 0;
            memcpy(&pm1a, (const uint8_t *)fadt + 64, 4);
            memcpy(&pm1b, (const uint8_t *)fadt + 68, 4);
            out->pm1a_cnt_port = pm1a;
            out->pm1b_cnt_port = pm1b;
        }

        uint32_t dsdt_phys = 0;
        if (fadt->length >= 148) {
            uint64_t x;
            memcpy(&x, (const uint8_t *)fadt + 140, 8);
            if (x != 0 && (x >> 32) == 0) dsdt_phys = (uint32_t)x;
        }
        if (dsdt_phys == 0 && fadt->length >= 44) {
            memcpy(&dsdt_phys, (const uint8_t *)fadt + 40, 4);
        }
        if (dsdt_phys != 0) {
            const AcpiSdtHeader *dsdt = (const AcpiSdtHeader *)(uintptr_t)dsdt_phys;
            if (sig_is(dsdt, "DSDT")) add_blob(out, dsdt);
        }
    }

    out->found = (out->n_blobs > 0);
}
