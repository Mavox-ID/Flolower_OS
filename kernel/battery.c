#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "acpi.h"
#include "aml.h"
#include "battery.h"

extern void serial_print(const char *);
extern void serial_print_hex(uint32_t);

static bool         s_inited = false;
static AcpiInfo      s_acpi;
static AmlNamespace  s_ns;
static AmlNode      *s_battery = NULL;

static void battery_lazy_init(void) {
    if (s_inited) return;
    s_inited = true;

    serial_print("battery: ACPI discovery starting\n");
    acpi_discover(&s_acpi);
    if (!s_acpi.found) {
        serial_print("battery: no ACPI tables found (no RSDP, or checksum failed)\n");
        return;
    }
    serial_print("battery: ACPI tables found, blob count = ");
    serial_print_hex((uint32_t)s_acpi.n_blobs);
    serial_print("\n");

    aml_build_namespace(&s_ns, &s_acpi);
    serial_print("battery: namespace built, node count = ");
    serial_print_hex((uint32_t)s_ns.n_nodes);
    serial_print("\n");

    s_battery = aml_find_device_by_hid(&s_ns, "PNP0C0A");
    if (!s_battery) {
        serial_print("battery: no device with _HID PNP0C0A found in namespace\n");
        return;
    }
    serial_print("battery: found PNP0C0A device node '");
    serial_print(s_battery->name);
    serial_print("'\n");
}

static AmlNode *find_method(AmlNode *device, const char *name4) {
    for (AmlNode *c = device->first_child; c; c = c->next_sibling) {
        if (c->type == AML_NODE_METHOD &&
            c->name[0] == name4[0] && c->name[1] == name4[1] &&
            c->name[2] == name4[2] && c->name[3] == name4[3]) {
            return c;
        }
    }
    return NULL;
}

bool battery_get_status(int *out_percent, bool *out_charging) {
    battery_lazy_init();
    if (!s_battery) return false;

    AmlNode *sta_m = find_method(s_battery, "_STA");
    if (sta_m) {
        AmlResult sta = aml_call_method(&s_ns, sta_m);
        if (!sta.ok || sta.is_package) {
            serial_print("battery: _STA evaluation failed (unsupported AML construct)\n");
            return false;
        }
        serial_print("battery: _STA = ");
        serial_print_hex((uint32_t)sta.int_value);
        serial_print("\n");
        if ((sta.int_value & 0x10) == 0) {
            serial_print("battery: _STA bit4 clear - slot present but no battery inserted\n");
            return false;
        }
    } else {
        serial_print("battery: no _STA method - assuming always present\n");
    }

    AmlNode *bst_m = find_method(s_battery, "_BST");
    AmlNode *bif_m = find_method(s_battery, "_BIF");
    if (!bst_m) serial_print("battery: no _BST method found\n");
    if (!bif_m) serial_print("battery: no _BIF method found\n");
    if (!bst_m || !bif_m) return false;

    AmlResult bst = aml_call_method(&s_ns, bst_m);
    if (!bst.ok || !bst.is_package || bst.pkg_count < 3) {
        serial_print("battery: _BST evaluation failed (unsupported AML construct in method body)\n");
        return false;
    }

    AmlResult bif = aml_call_method(&s_ns, bif_m);
    if (!bif.ok || !bif.is_package || bif.pkg_count < 3) {
        serial_print("battery: _BIF evaluation failed (unsupported AML construct in method body)\n");
        return false;
    }

    uint64_t state             = bst.pkg_ints[0];
    uint64_t remaining_cap     = bst.pkg_ints[2];
    uint64_t last_full_cap     = bif.pkg_ints[2];

    if (last_full_cap == 0) {
        serial_print("battery: _BIF LastFullChargeCapacity is 0\n");
        return false;
    }
    if (remaining_cap == 0xFFFFFFFFu || last_full_cap == 0xFFFFFFFFu) {
        serial_print("battery: capacity reported as 'unknown' (0xFFFFFFFF) per spec\n");
        return false;
    }

    uint64_t percent = (remaining_cap * 100) / last_full_cap;
    if (percent > 100) percent = 100;

    serial_print("battery: OK, percent = ");
    serial_print_hex((uint32_t)percent);
    serial_print("\n");

    *out_percent = (int)percent;
    *out_charging = (state & 0x2) != 0;
    return true;
}
