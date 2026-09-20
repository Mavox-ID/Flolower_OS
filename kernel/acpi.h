#ifndef ACPI_H
#define ACPI_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const uint8_t *aml;
    uint32_t       len;
} AcpiAmlBlob;

#define ACPI_MAX_BLOBS 8

typedef struct {
    bool        found;
    AcpiAmlBlob blobs[ACPI_MAX_BLOBS];
    int         n_blobs;

    uint32_t    pm1a_cnt_port;
    uint32_t    pm1b_cnt_port;
} AcpiInfo;

void acpi_discover(AcpiInfo *out);

#endif
