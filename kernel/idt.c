#include <stdint.h>

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void idt_flush(uint32_t);

#define DECLARE_ISR(n) extern void isr##n(void);
DECLARE_ISR(0)  DECLARE_ISR(1)  DECLARE_ISR(2)  DECLARE_ISR(3)
DECLARE_ISR(4)  DECLARE_ISR(5)  DECLARE_ISR(6)  DECLARE_ISR(7)
DECLARE_ISR(8)  DECLARE_ISR(9)  DECLARE_ISR(10) DECLARE_ISR(11)
DECLARE_ISR(12) DECLARE_ISR(13) DECLARE_ISR(14) DECLARE_ISR(15)
DECLARE_ISR(16) DECLARE_ISR(17) DECLARE_ISR(18) DECLARE_ISR(19)
DECLARE_ISR(20) DECLARE_ISR(21) DECLARE_ISR(22) DECLARE_ISR(23)
DECLARE_ISR(24) DECLARE_ISR(25) DECLARE_ISR(26) DECLARE_ISR(27)
DECLARE_ISR(28) DECLARE_ISR(29) DECLARE_ISR(30) DECLARE_ISR(31)

#define DECLARE_IRQ(n) extern void irq##n(void);
DECLARE_IRQ(0)  DECLARE_IRQ(1)  DECLARE_IRQ(2)  DECLARE_IRQ(3)
DECLARE_IRQ(4)  DECLARE_IRQ(5)  DECLARE_IRQ(6)  DECLARE_IRQ(7)
DECLARE_IRQ(8)  DECLARE_IRQ(9)  DECLARE_IRQ(10) DECLARE_IRQ(11)
DECLARE_IRQ(12) DECLARE_IRQ(13) DECLARE_IRQ(14) DECLARE_IRQ(15)

static void set_gate(int n, uint32_t base) {
    idt[n].base_lo  = base & 0xFFFF;
    idt[n].base_hi  = (base >> 16) & 0xFFFF;
    idt[n].sel      = 0x08;
    idt[n].always0  = 0;
    idt[n].flags    = 0x8E;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    set_gate(0, (uint32_t)isr0);   set_gate(1, (uint32_t)isr1);
    set_gate(2, (uint32_t)isr2);   set_gate(3, (uint32_t)isr3);
    set_gate(4, (uint32_t)isr4);   set_gate(5, (uint32_t)isr5);
    set_gate(6, (uint32_t)isr6);   set_gate(7, (uint32_t)isr7);
    set_gate(8, (uint32_t)isr8);   set_gate(9, (uint32_t)isr9);
    set_gate(10, (uint32_t)isr10); set_gate(11, (uint32_t)isr11);
    set_gate(12, (uint32_t)isr12); set_gate(13, (uint32_t)isr13);
    set_gate(14, (uint32_t)isr14); set_gate(15, (uint32_t)isr15);
    set_gate(16, (uint32_t)isr16); set_gate(17, (uint32_t)isr17);
    set_gate(18, (uint32_t)isr18); set_gate(19, (uint32_t)isr19);
    set_gate(20, (uint32_t)isr20); set_gate(21, (uint32_t)isr21);
    set_gate(22, (uint32_t)isr22); set_gate(23, (uint32_t)isr23);
    set_gate(24, (uint32_t)isr24); set_gate(25, (uint32_t)isr25);
    set_gate(26, (uint32_t)isr26); set_gate(27, (uint32_t)isr27);
    set_gate(28, (uint32_t)isr28); set_gate(29, (uint32_t)isr29);
    set_gate(30, (uint32_t)isr30); set_gate(31, (uint32_t)isr31);

    set_gate(32, (uint32_t)irq0);  set_gate(33, (uint32_t)irq1);
    set_gate(34, (uint32_t)irq2);  set_gate(35, (uint32_t)irq3);
    set_gate(36, (uint32_t)irq4);  set_gate(37, (uint32_t)irq5);
    set_gate(38, (uint32_t)irq6);  set_gate(39, (uint32_t)irq7);
    set_gate(40, (uint32_t)irq8);  set_gate(41, (uint32_t)irq9);
    set_gate(42, (uint32_t)irq10); set_gate(43, (uint32_t)irq11);
    set_gate(44, (uint32_t)irq12); set_gate(45, (uint32_t)irq13);
    set_gate(46, (uint32_t)irq14); set_gate(47, (uint32_t)irq15);

    idt_flush((uint32_t)&idtp);
}
