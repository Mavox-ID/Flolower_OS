#include <stdint.h>
#include <stdbool.h>

extern void serial_init(void);
extern void serial_print(const char *);
extern void serial_print_hex(uint32_t);

extern void pic_remap(void);
extern void idt_init(void);
extern void pit_init(uint32_t freq);
extern void ps2_init(void);
extern bool vesa_init(int width, int height);

extern int main(int argc, char **argv);

extern void process_init(void);
extern int  process_create(const char *name, void (*entry)(void));

#ifdef PROCESS_TEST
static volatile int counter_a = 0;
static volatile int counter_b = 0;
static void test_process_a(void) {
    for (;;) { counter_a++; for (volatile int i = 0; i < 500000; i++) { } }
}
static void test_process_b(void) {
    for (;;) { counter_b++; for (volatile int i = 0; i < 500000; i++) { } }
}
#endif

void kmain(void) {
    serial_init();
    serial_print("Flolower kernel: booting\n");

    pic_remap();
    idt_init();
    pit_init(1000);
    ps2_init();
    serial_print("Flolower kernel: PIC/IDT/PIT/PS2 ready\n");

    if (!vesa_init(800, 600)) {
        serial_print("Flolower kernel: VESA VBE init FAILED - no display, halting\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    serial_print("Flolower kernel: framebuffer ready (800x600x32)\n");

#ifdef REPO_TEST
    {
        extern bool repo_find(const char *name, void *out_entry);
        extern int  repo_fetch(const void *entry, char *glyph_out, char *content_out, int content_out_size);
        unsigned char entry_buf[40];
        char glyph[4] = {0};
        static char content[2048];
        if (repo_find("poem", entry_buf)) {
            serial_print("repo_test: found 'poem' in index\n");
            int n = repo_fetch(entry_buf, glyph, content, sizeof(content));
            serial_print("repo_test: fetched bytes, first line: ");
            for (int i = 0; i < n && content[i] != '\n'; i++) {
                char c[2] = { content[i], 0 };
                serial_print(c);
            }
            serial_print("\n");
        } else {
            serial_print("repo_test: 'poem' NOT found in index\n");
        }
    }
#endif

    process_init();
    serial_print("Flolower kernel: process subsystem ready\n");

#ifdef PROCESS_TEST
    process_create("test_a", test_process_a);
    process_create("test_b", test_process_b);
#endif

    __asm__ volatile("sti");

#ifdef PROCESS_TEST
    {
        extern void timer_delay_ms(int ms);
        for (int i = 0; i < 15; i++) {
            timer_delay_ms(100);
            serial_print("process_test: counter_a=");
            serial_print_hex((uint32_t)counter_a);
            serial_print(" counter_b=");
            serial_print_hex((uint32_t)counter_b);
            serial_print("\n");
        }
    }
#endif

    serial_print("Flolower kernel: entering shell main()\n");

    main(0, (char **)0);

    serial_print("Flolower kernel: shell main() returned, halting\n");
    for (;;) __asm__ volatile("cli; hlt");
}
