#include <stdint.h>
#include <stdbool.h>
#include "io.h"

#define KBD_BUF_SIZE 64
static volatile uint8_t kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0, kbd_tail = 0;

void kbd_irq_handler(void) {
    uint8_t sc = inb(0x60);
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) { kbd_buf[kbd_head] = sc; kbd_head = next; }
}

bool kbd_read_scancode(uint8_t *out) {
    if (kbd_tail == kbd_head) return false;
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return true;
}

#define MOUSE_BUF_SIZE 32
typedef struct { int dx, dy; uint8_t buttons; } MousePacket;
static volatile MousePacket mouse_buf[MOUSE_BUF_SIZE];
static volatile int mouse_head = 0, mouse_tail = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];

void mouse_irq_handler(void) {
    uint8_t data = inb(0x60);
    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) return;
            mouse_bytes[0] = data;
            mouse_cycle = 1;
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle = 2;
            break;
        case 2: {
            mouse_bytes[2] = data;
            mouse_cycle = 0;
            uint8_t flags = mouse_bytes[0];
            int dx = (int8_t)mouse_bytes[1];
            int dy = (int8_t)mouse_bytes[2];
            uint8_t buttons = flags & 0x07;
            int next = (mouse_head + 1) % MOUSE_BUF_SIZE;
            if (next != mouse_tail) {
                mouse_buf[mouse_head].dx = dx;
                mouse_buf[mouse_head].dy = dy;
                mouse_buf[mouse_head].buttons = buttons;
                mouse_head = next;
            }
            break;
        }
    }
}

bool mouse_read_packet(int *dx, int *dy, uint8_t *buttons) {
    if (mouse_tail == mouse_head) return false;
    *dx = mouse_buf[mouse_tail].dx;
    *dy = mouse_buf[mouse_tail].dy;
    *buttons = mouse_buf[mouse_tail].buttons;
    mouse_tail = (mouse_tail + 1) % MOUSE_BUF_SIZE;
    return true;
}

static void ps2_wait_input(void)  { while (inb(0x64) & 0x02) { } }
static void ps2_wait_output(void) { while (!(inb(0x64) & 0x01)) { } }

static void mouse_write(uint8_t val) {
    ps2_wait_input();
    outb(0x64, 0xD4);
    ps2_wait_input();
    outb(0x60, val);
}
static uint8_t mouse_read_ack(void) {
    ps2_wait_output();
    return inb(0x60);
}

void ps2_init(void) {
    ps2_wait_input(); outb(0x64, 0xA8);

    ps2_wait_input(); outb(0x64, 0x20);
    ps2_wait_output();
    uint8_t status = inb(0x60);
    status |= 0x02;
    status &= ~0x20;
    ps2_wait_input(); outb(0x64, 0x60);
    ps2_wait_input(); outb(0x60, status);

    mouse_write(0xF6); mouse_read_ack();
    mouse_write(0xF4); mouse_read_ack();
}
