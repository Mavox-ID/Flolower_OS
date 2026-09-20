#include <stdint.h>
#include "io.h"

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} regs_t;

extern void serial_print(const char *);
extern void serial_print_hex(uint32_t);
extern void pit_tick(void);
extern void kbd_irq_handler(void);
extern void mouse_irq_handler(void);
extern void scheduler_tick(void);

void isr_handler(regs_t *r) {
    serial_print("EXCEPTION int=");
    serial_print_hex(r->int_no);
    serial_print(" err=");
    serial_print_hex(r->err_code);
    serial_print(" eip=");
    serial_print_hex(r->eip);
    serial_print("\n");
    for (;;) __asm__ volatile("cli; hlt");
}

void irq_handler(regs_t *r) {
    if (r->int_no >= 40) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    switch (r->int_no) {
        case 32: {
            pit_tick();
            static int since_last_switch = 0;
            if (++since_last_switch >= 10) {
                since_last_switch = 0;
                scheduler_tick();
            }
            break;
        }
        case 33: kbd_irq_handler(); break;
        case 44: mouse_irq_handler(); break;
        default: break;
    }
}
