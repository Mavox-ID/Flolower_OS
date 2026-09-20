#include <stdint.h>
#include <stdbool.h>
#include "io.h"
#include "pci.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                        ((uint32_t)func << 8) | (offset & 0xFC) | 0x80000000;
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                        ((uint32_t)func << 8) | (offset & 0xFC) | 0x80000000;
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

bool pci_find_vga(PciAddr *out) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t id = pci_config_read32((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = id & 0xFFFF;
            if (vendor == 0xFFFF) continue;

            uint32_t classreg = pci_config_read32((uint8_t)bus, (uint8_t)slot, 0, 0x08);
            uint8_t classcode = (classreg >> 24) & 0xFF;
            if (classcode == 0x03) {
                out->bus = (uint8_t)bus;
                out->slot = (uint8_t)slot;
                out->func = 0;
                return true;
            }
        }
    }
    return false;
}

uint32_t pci_bar0(PciAddr a) {
    return pci_config_read32(a.bus, a.slot, a.func, 0x10) & 0xFFFFFFF0;
}

void pci_enable_memory_space(PciAddr a) {
    uint32_t cmd = pci_config_read32(a.bus, a.slot, a.func, 0x04);
    cmd |= 0x02;
    pci_config_write32(a.bus, a.slot, a.func, 0x04, cmd);
}
