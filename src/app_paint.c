#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdlib.h>

#define CANVAS_W 260
#define CANVAS_H 170
#define BRUSH    4

typedef struct {
    PColor canvas[CANVAS_H][CANVAS_W];
    PColor current_color;
} PaintState;

static const PColor PALETTE[] = {
    { 0xff, 0xff, 0xff },
    { 0xff, 0x44, 0x44 },
    { 0x4d, 0xd9, 0x6b },
    { 0x4d, 0x7f, 0xff },
    { 0xff, 0xd4, 0x4d },
    { 0x0f, 0x0b, 0x18 },
};
#define N_PALETTE (int)(sizeof(PALETTE) / sizeof(PALETTE[0]))

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static PRect toolbar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, w->rect.w, 34 };
}
static PRect swatch_rect(AppWindow *w, int i) {
    PRect tb = toolbar_rect(w);
    return (PRect){ tb.x + 6 + i * 30, tb.y + 5, 24, 24 };
}
static PRect clear_btn_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ tb.x + tb.w - 70, tb.y + 5, 62, 24 };
}
static PRect canvas_screen_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ w->rect.x + 6, tb.y + tb.h + 4, CANVAS_W, CANVAS_H };
}

static void paint_clear(PaintState *p) {
    PColor bg = { 0x0c, 0x0a, 0x12 };
    for (int y = 0; y < CANVAS_H; y++)
        for (int x = 0; x < CANVAS_W; x++)
            p->canvas[y][x] = bg;
}

static void paint_render(AppWindow *w) {
    PaintState *p = (PaintState *)w->user_data;

    PRect tb = toolbar_rect(w);
    platform_fill_rect(tb, color_menu_item());

    for (int i = 0; i < N_PALETTE; i++) {
        PRect sw = swatch_rect(w, i);
        platform_fill_rect(sw, PALETTE[i]);
        bool selected = p->current_color.r == PALETTE[i].r &&
                         p->current_color.g == PALETTE[i].g &&
                         p->current_color.b == PALETTE[i].b;
        if (selected) platform_draw_rect_outline(sw, color_accent());
    }

    PRect clear_btn = clear_btn_rect(w);
    platform_fill_rect(clear_btn, color_task_active());
    draw_text_centered("Clear", clear_btn, color_text());

    PRect canvas = canvas_screen_rect(w);
    platform_draw_rect_outline((PRect){ canvas.x - 1, canvas.y - 1, canvas.w + 2, canvas.h + 2 }, color_title_bg());

    for (int y = 0; y < CANVAS_H; y += BRUSH) {
        for (int x = 0; x < CANVAS_W; x += BRUSH) {
            PRect px = { canvas.x + x, canvas.y + y, BRUSH, BRUSH };
            platform_fill_rect(px, p->canvas[y][x]);
        }
    }
}

static void paint_at(PaintState *p, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= CANVAS_W || cy >= CANVAS_H) return;
    int bx = (cx / BRUSH) * BRUSH;
    int by = (cy / BRUSH) * BRUSH;
    for (int y = by; y < by + BRUSH * 2 && y < CANVAS_H; y += BRUSH) {
        for (int x = bx; x < bx + BRUSH * 2 && x < CANVAS_W; x += BRUSH) {
            p->canvas[y][x] = p->current_color;
        }
    }
}

static void paint_handle_point(AppWindow *w, int lx, int ly) {
    PaintState *p = (PaintState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    for (int i = 0; i < N_PALETTE; i++) {
        if (point_in(swatch_rect(w, i), mx, my)) { p->current_color = PALETTE[i]; return; }
    }
    if (point_in(clear_btn_rect(w), mx, my)) { paint_clear(p); return; }

    PRect canvas = canvas_screen_rect(w);
    if (point_in(canvas, mx, my)) {
        paint_at(p, mx - canvas.x, my - canvas.y);
    }
}

static void paint_click(AppWindow *w, int lx, int ly) { paint_handle_point(w, lx, ly); }
static void paint_drag(AppWindow *w, int lx, int ly)  { paint_handle_point(w, lx, ly); }

void app_open_paint(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Paint", 0.4f, 0.45f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    PaintState *p = calloc(1, sizeof(PaintState));
    paint_clear(p);
    p->current_color = PALETTE[0];

    w->user_data = p;
    w->on_render = paint_render;
    w->on_click = paint_click;
    w->on_drag = paint_drag;
}
