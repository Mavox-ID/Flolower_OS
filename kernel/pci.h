#ifndef PCI_H
#define PCI_H
#include <stdint.h>
#include <stdbool.h>

typedef struct { uint8_t bus, slot, func; } PciAddr;

bool     pci_find_vga(PciAddr *out);
uint32_t pci_bar0(PciAddr a);
void     pci_enable_memory_space(PciAddr a);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif
