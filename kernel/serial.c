#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int serial_tx_empty(void) { return inb(COM1 + 5) & 0x20; }

void serial_putc(char c) {
    while (!serial_tx_empty());
    outb(COM1, (uint8_t)c);
}

void serial_print(const char *s) {
    while (*s) serial_putc(*s++);
}

void serial_print_hex(uint32_t v) {
    char buf[9];
    buf[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nib = v & 0xF;
        buf[i] = nib < 10 ? ('0' + nib) : ('A' + nib - 10);
        v >>= 4;
    }
    serial_print(buf);
}
