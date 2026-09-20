#include <stdint.h>
#include "io.h"

static volatile uint32_t pit_ticks = 0;
static uint32_t pit_freq = 1000;

void pit_init(uint32_t freq) {
    pit_freq = freq;
    uint32_t divisor = 1193182 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_tick(void) { pit_ticks++; }

uint32_t timer_ticks_ms(void) {
    return pit_ticks;
}

void timer_delay_ms(int ms) {
    uint32_t target = pit_ticks + (uint32_t)ms;
    while (pit_ticks < target) __asm__ volatile("sti; hlt");
}
