#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VBE_PARAMS_ADDR 0x0600

typedef struct {
    uint32_t lfb_phys_addr;
    uint16_t bytes_per_scanline;
    uint16_t mode_set_ok;
    uint8_t  red_pos;
    uint8_t  green_pos;
    uint8_t  blue_pos;
} __attribute__((packed)) VbeHandoff;

#define MAX_FB_W 800
#define MAX_FB_H 600

extern void serial_print(const char *);
extern void serial_print_hex(uint32_t);

static uint32_t backbuffer[MAX_FB_W * MAX_FB_H];

uint32_t *fb_backbuffer = backbuffer;
int fb_width = 0;
int fb_height = 0;
int fb_red_pos = 16, fb_green_pos = 8, fb_blue_pos = 0;

static volatile uint8_t *g_lfb = NULL;
static uint32_t g_pitch = 0;

bool vesa_init(int width, int height) {
    const VbeHandoff *handoff = (const VbeHandoff *)VBE_PARAMS_ADDR;

    if (!handoff->mode_set_ok) {
        serial_print("vesa_init: BIOS did not accept the requested VBE mode\n");
        return false;
    }

    uint32_t lfb_phys = handoff->lfb_phys_addr;
    g_pitch = handoff->bytes_per_scanline;

    serial_print("vesa_init: LFB physical address = ");
    serial_print_hex(lfb_phys);
    serial_print("\n");
    serial_print("vesa_init: bytes per scanline = ");
    serial_print_hex((uint32_t)g_pitch);
    serial_print("\n");

    if (lfb_phys == 0 || g_pitch == 0) {
        serial_print("vesa_init: handoff struct looks empty — boot.asm VBE call likely failed\n");
        return false;
    }

    g_lfb = (volatile uint8_t *)lfb_phys;
    fb_width = width;
    fb_height = height;
    fb_red_pos = handoff->red_pos;
    fb_green_pos = handoff->green_pos;
    fb_blue_pos = handoff->blue_pos;
    serial_print("vesa_init: R/G/B field positions = ");
    serial_print_hex(fb_red_pos);
    serial_print(" / ");
    serial_print_hex(fb_green_pos);
    serial_print(" / ");
    serial_print_hex(fb_blue_pos);
    serial_print("\n");
    return true;
}

void fb_present(void) {
    if (!g_lfb) return;
    int red_byte = fb_red_pos / 8;
    int green_byte = fb_green_pos / 8;
    int blue_byte = fb_blue_pos / 8;

    for (int y = 0; y < fb_height; y++) {
        uint8_t *dst = (uint8_t *)(g_lfb + (uint32_t)y * g_pitch);
        const uint32_t *src = &backbuffer[y * fb_width];
        for (int x = 0; x < fb_width; x++) {
            uint32_t px = src[x];
            dst[x * 3 + red_byte]   = (uint8_t)(px >> fb_red_pos);
            dst[x * 3 + green_byte] = (uint8_t)(px >> fb_green_pos);
            dst[x * 3 + blue_byte]  = (uint8_t)(px >> fb_blue_pos);
        }
    }
}
