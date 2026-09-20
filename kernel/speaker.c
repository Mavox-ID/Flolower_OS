#include <stdint.h>
#include "io.h"

extern void timer_delay_ms(int ms);

void speaker_beep(int freq_hz, int duration_ms) {
    if (freq_hz <= 0 || duration_ms <= 0) return;

    uint32_t div = 1193182u / (uint32_t)freq_hz;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 3);

    timer_delay_ms(duration_ms);

    tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}
