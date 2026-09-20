#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "acpi.h"
#include "aml.h"
#include "io.h"
#include "power.h"

extern void serial_print(const char *);
extern void serial_print_hex(uint32_t);

#define SLP_EN_BIT (1u << 13)

static AmlNode *find_named_anywhere(AmlNode *node, const char *name4) {
    if (node->name[0] == name4[0] && node->name[1] == name4[1] &&
        node->name[2] == name4[2] && node->name[3] == name4[3]) {
        return node;
    }
    for (AmlNode *c = node->first_child; c; c = c->next_sibling) {
        AmlNode *found = find_named_anywhere(c, name4);
        if (found) return found;
    }
    return NULL;
}

static void halt_forever(void) {
    for (;;) __asm__ volatile("cli; hlt");
}

void system_shutdown(void) {
    serial_print("shutdown: ACPI power-off starting\n");

    AcpiInfo acpi;
    acpi_discover(&acpi);
    if (!acpi.found) {
        serial_print("shutdown: no ACPI tables found - halting instead\n");
        halt_forever();
    }
    if (acpi.pm1a_cnt_port == 0) {
        serial_print("shutdown: FADT has no PM1a_CNT_BLK - halting instead\n");
        halt_forever();
    }

    AmlNamespace ns;
    aml_build_namespace(&ns, &acpi);

    AmlNode *s5 = find_named_anywhere(ns.root, "_S5_");
    if (!s5 || s5->type != AML_NODE_NAME_PKG || s5->pkg_count < 1) {
        serial_print("shutdown: \\_S5_ package not found/understood - halting instead\n");
        halt_forever();
    }

    uint8_t slp_typa = (uint8_t)(s5->pkg_ints[0] & 0x7);
    uint8_t slp_typb = (uint8_t)((s5->pkg_count > 1 ? s5->pkg_ints[1] : s5->pkg_ints[0]) & 0x7);
    serial_print("shutdown: SLP_TYPa = ");
    serial_print_hex(slp_typa);
    serial_print("\n");

    uint16_t porta = (uint16_t)acpi.pm1a_cnt_port;
    uint16_t cur_a = inw(porta);
    uint16_t val_a = (uint16_t)((cur_a & ~(0x7u << 10)) | ((uint32_t)slp_typa << 10) | SLP_EN_BIT);
    outw(porta, val_a);

    if (acpi.pm1b_cnt_port != 0) {
        uint16_t portb = (uint16_t)acpi.pm1b_cnt_port;
        uint16_t cur_b = inw(portb);
        uint16_t val_b = (uint16_t)((cur_b & ~(0x7u << 10)) | ((uint32_t)slp_typb << 10) | SLP_EN_BIT);
        outw(portb, val_b);
    }

    serial_print("shutdown: PM1_CNT written, machine should be off now\n");
    halt_forever();
}
