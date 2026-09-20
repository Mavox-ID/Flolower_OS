#include <stdint.h>
#include "io.h"

void system_restart(void) {
    uint32_t spins = 0;
    while ((inb(0x64) & 0x02) && spins < 1000000) spins++;
    outb(0x64, 0xFE);

    for (;;) __asm__ volatile("cli; hlt");
}
