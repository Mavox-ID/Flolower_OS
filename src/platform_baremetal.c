#include "platform.h"
#include "font8x8_basic.h"
#include "font8x8_cyrillic.h"
#include "utf8.h"
#include <stddef.h>

extern uint32_t *fb_backbuffer;
extern int       fb_width;
extern int       fb_height;
extern int       fb_red_pos, fb_green_pos, fb_blue_pos;
extern void fb_present(void);

extern bool kbd_read_scancode(uint8_t *out_scancode);

extern bool mouse_read_packet(int *dx, int *dy, uint8_t *buttons);

extern uint32_t timer_ticks_ms(void);

extern void timer_delay_ms(int ms);

static int g_w = 0, g_h = 0;
static int g_mouse_x = 0, g_mouse_y = 0;
static uint8_t g_prev_buttons = 0;
static bool g_shift_down = false;
static bool g_ctrl_down = false;
static bool g_ext_prefix = false;
static int  g_volume = 100;

void platform_set_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_volume = percent;
}
int platform_get_volume(void) { return g_volume; }

extern bool battery_get_status(int *out_percent, bool *out_charging);

bool platform_battery_status(int *out_percent, bool *out_charging) {
    return battery_get_status(out_percent, out_charging);
}

bool platform_init(int width, int height, const char *title) {
    (void)title;
    g_w = width; g_h = height;
    if (fb_width > 0)  g_w = fb_width;
    if (fb_height > 0) g_h = fb_height;
    g_mouse_x = g_w / 2;
    g_mouse_y = g_h / 2;
    return fb_backbuffer != NULL;
}

void platform_shutdown(void) {  }

int platform_width(void)  { return g_w; }
int platform_height(void) { return g_h; }

uint32_t platform_ticks_ms(void)     { return timer_ticks_ms(); }
void     platform_delay_ms(int ms)   { timer_delay_ms(ms); }

static inline void put_pixel(int x, int y, PColor c) {
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return;
    fb_backbuffer[y * g_w + x] = ((uint32_t)c.r << fb_red_pos) | ((uint32_t)c.g << fb_green_pos) | ((uint32_t)c.b << fb_blue_pos);
}

void platform_clear(PColor c) {
    uint32_t packed = ((uint32_t)c.r << fb_red_pos) | ((uint32_t)c.g << fb_green_pos) | ((uint32_t)c.b << fb_blue_pos);
    int n = g_w * g_h;
    for (int i = 0; i < n; i++) fb_backbuffer[i] = packed;
}

void platform_fill_rect(PRect r, PColor c) {
    int x0 = r.x < 0 ? 0 : r.x;
    int y0 = r.y < 0 ? 0 : r.y;
    int x1 = r.x + r.w; if (x1 > g_w) x1 = g_w;
    int y1 = r.y + r.h; if (y1 > g_h) y1 = g_h;
    uint32_t packed = ((uint32_t)c.r << fb_red_pos) | ((uint32_t)c.g << fb_green_pos) | ((uint32_t)c.b << fb_blue_pos);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = &fb_backbuffer[y * g_w];
        for (int x = x0; x < x1; x++) row[x] = packed;
    }
}

void platform_draw_rect_outline(PRect r, PColor c) {
    PRect top    = { r.x, r.y, r.w, 1 };
    PRect bottom = { r.x, r.y + r.h - 1, r.w, 1 };
    PRect left   = { r.x, r.y, 1, r.h };
    PRect right  = { r.x + r.w - 1, r.y, 1, r.h };
    platform_fill_rect(top, c);
    platform_fill_rect(bottom, c);
    platform_fill_rect(left, c);
    platform_fill_rect(right, c);
}

void platform_present(void) { fb_present(); }

#define GLYPH_W 8
#define GLYPH_H 8

int platform_text_width(const char *s) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        p += utf8_decode(p, &cp);
        n++;
    }
    return n * GLYPH_W;
}
int platform_text_height(void) { return GLYPH_H; }

static const unsigned char *glyph_rows_for(unsigned int cp) {
    if (cp < 128) return (const unsigned char *)font8x8_basic[cp];
    const unsigned char *rows = cyrillic_glyph_rows(cp);
    return rows ? rows : (const unsigned char *)font8x8_basic[(int)'?'];
}

void platform_draw_text(const char *s, int x, int y, PColor c) {
    int cx = x;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        p += utf8_decode(p, &cp);
        const unsigned char *rows = glyph_rows_for(cp);
        for (int row = 0; row < 8; row++) {
            unsigned char bits = rows[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (1 << col)) put_pixel(cx + col, y + row, c);
            }
        }
        cx += GLYPH_W;
    }
}

static const char SC_ASCII_LOWER[] =
    "\0\0331234567890-=\b\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char SC_ASCII_UPPER[] =
    "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";

static bool decode_scancode(uint8_t sc, PEvent *out) {
    bool release = (sc & 0x80) != 0;
    uint8_t code = sc & 0x7F;

    if (g_ext_prefix) {
        g_ext_prefix = false;
        if (release) return false;
        switch (code) {
            case 0x4B: out->type = PEV_KEY_DOWN; out->key = PK_LEFT;  return true;
            case 0x4D: out->type = PEV_KEY_DOWN; out->key = PK_RIGHT; return true;
            case 0x48: out->type = PEV_KEY_DOWN; out->key = PK_UP;    return true;
            case 0x50: out->type = PEV_KEY_DOWN; out->key = PK_DOWN;  return true;
            case 0x53: out->type = PEV_KEY_DOWN; out->key = PK_DELETE;return true;
            default: return false;
        }
    }
    if (sc == 0xE0) { g_ext_prefix = true; return false; }

    if (code == 0x2A || code == 0x36) { g_shift_down = !release; return false; }
    if (code == 0x1D) { g_ctrl_down = !release; return false; }
    if (release) return false;

    switch (code) {
        case 0x01: out->type = PEV_KEY_DOWN; out->key = PK_ESCAPE;    return true;
        case 0x0E: out->type = PEV_KEY_DOWN; out->key = PK_BACKSPACE; return true;
        case 0x1C: out->type = PEV_KEY_DOWN; out->key = PK_RETURN;    return true;
        case 0x0F: out->type = PEV_KEY_DOWN; out->key = PK_TAB;       return true;
        case 0x1F:
            if (g_ctrl_down) { out->type = PEV_KEY_DOWN; out->key = PK_S; out->mods = PMOD_CTRL; return true; }
            break;
        default: break;
    }

    if (code < sizeof(SC_ASCII_LOWER) - 1) {
        char ch = g_shift_down ? SC_ASCII_UPPER[code] : SC_ASCII_LOWER[code];
        if (ch) {
            out->type = PEV_TEXT_INPUT;
            out->text[0] = ch;
            out->text[1] = '\0';
            return true;
        }
    }
    return false;
}

extern void speaker_beep(int freq_hz, int duration_ms);
void platform_beep(int freq_hz, int duration_ms) {
    if (g_volume <= 0) return;
    speaker_beep(freq_hz, duration_ms);
}

bool platform_poll_event(PEvent *out) {
    out->x = out->y = out->button = out->wheel_dy = out->mods = 0;
    out->key = PK_NONE;
    out->text[0] = '\0';

    uint8_t sc;
    if (kbd_read_scancode(&sc)) {
        out->type = PEV_NONE;
        if (decode_scancode(sc, out)) return true;
        return false;
    }

    int dx, dy; uint8_t buttons;
    if (mouse_read_packet(&dx, &dy, &buttons)) {
        g_mouse_x += dx; if (g_mouse_x < 0) g_mouse_x = 0; if (g_mouse_x >= g_w) g_mouse_x = g_w - 1;
        g_mouse_y -= dy; if (g_mouse_y < 0) g_mouse_y = 0; if (g_mouse_y >= g_h) g_mouse_y = g_h - 1;

        uint8_t changed = buttons ^ g_prev_buttons;
        uint8_t pressed = changed & buttons;
        uint8_t released = changed & g_prev_buttons;
        g_prev_buttons = buttons;

        out->x = g_mouse_x; out->y = g_mouse_y;
        if (pressed & 0x01) { out->type = PEV_MOUSE_DOWN; out->button = PBTN_LEFT; return true; }
        if (pressed & 0x02) { out->type = PEV_MOUSE_DOWN; out->button = PBTN_RIGHT; return true; }
        if (released & 0x01) { out->type = PEV_MOUSE_UP; out->button = PBTN_LEFT; return true; }
        if (released & 0x02) { out->type = PEV_MOUSE_UP; out->button = PBTN_RIGHT; return true; }
        out->type = PEV_MOUSE_MOVE;
        return true;
    }

    return false;
}
