#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct { uint8_t r, g, b; } PColor;
typedef struct { int x, y, w, h; } PRect;

typedef enum {
    PK_NONE = 0,
    PK_LEFT, PK_RIGHT, PK_UP, PK_DOWN,
    PK_RETURN, PK_BACKSPACE, PK_DELETE, PK_ESCAPE, PK_TAB,
    PK_S,
    PK_OTHER
} PKeycode;

typedef enum {
    PEV_NONE = 0,
    PEV_MOUSE_MOVE,
    PEV_MOUSE_DOWN,
    PEV_MOUSE_UP,
    PEV_MOUSE_WHEEL,
    PEV_KEY_DOWN,
    PEV_TEXT_INPUT,
    PEV_QUIT,
    PEV_RESIZE
} PEventType;

#define PBTN_LEFT   1
#define PBTN_RIGHT  2
#define PMOD_CTRL   1

typedef struct {
    PEventType type;
    int      x, y;
    int      button;
    int      wheel_dy;
    PKeycode key;
    int      mods;
    char     text[8];
} PEvent;

bool     platform_init(int width, int height, const char *title);
void     platform_shutdown(void);
int      platform_width(void);
int      platform_height(void);

uint32_t platform_ticks_ms(void);
void     platform_delay_ms(int ms);

void     platform_clear(PColor c);
void     platform_fill_rect(PRect r, PColor c);
void     platform_draw_rect_outline(PRect r, PColor c);
void     platform_present(void);

int      platform_text_width(const char *s);
int      platform_text_height(void);
void     platform_draw_text(const char *s, int x, int y, PColor c);

bool     platform_poll_event(PEvent *out);

void     platform_beep(int freq_hz, int duration_ms);

void     platform_set_volume(int percent);
int      platform_get_volume(void);

bool     platform_battery_status(int *out_percent, bool *out_charging);

#endif
