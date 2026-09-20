#include "shell.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t started_at_ms;
    uint32_t accumulated_ms;
    bool running;
} TimerState;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static uint32_t timer_elapsed_ms(TimerState *t) {
    uint32_t total = t->accumulated_ms;
    if (t->running) total += platform_ticks_ms() - t->started_at_ms;
    return total;
}

static PRect display_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 10, w->rect.y + TITLEBAR_H + 16, w->rect.w - 20, 50 };
}
static PRect start_btn_rect(AppWindow *w) {
    PRect d = display_rect(w);
    return (PRect){ d.x, d.y + d.h + 14, (d.w - 12) / 2, 40 };
}
static PRect reset_btn_rect(AppWindow *w) {
    PRect s = start_btn_rect(w);
    return (PRect){ s.x + s.w + 12, s.y, s.w, s.h };
}

static void timer_render(AppWindow *w) {
    TimerState *t = (TimerState *)w->user_data;

    PRect disp = display_rect(w);
    platform_fill_rect(disp, color_menu_item());

    uint32_t ms = timer_elapsed_ms(t);
    uint32_t total_sec = ms / 1000;
    uint32_t mm = total_sec / 60;
    uint32_t ss = total_sec % 60;
    uint32_t tenths = (ms % 1000) / 100;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02u:%02u.%u", mm, ss, tenths);
    draw_text_centered(buf, disp, color_text());

    PRect start_btn = start_btn_rect(w);
    PRect reset_btn = reset_btn_rect(w);
    platform_fill_rect(start_btn, t->running ? color_danger() : color_task_active());
    draw_text_centered(t->running ? "Stop" : "Start", start_btn, color_text());
    platform_fill_rect(reset_btn, color_menu_item());
    draw_text_centered("Reset", reset_btn, color_text());
}

static void timer_click(AppWindow *w, int lx, int ly) {
    TimerState *t = (TimerState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    if (point_in(start_btn_rect(w), mx, my)) {
        if (t->running) {
            t->accumulated_ms += platform_ticks_ms() - t->started_at_ms;
            t->running = false;
        } else {
            t->started_at_ms = platform_ticks_ms();
            t->running = true;
        }
        return;
    }
    if (point_in(reset_btn_rect(w), mx, my)) {
        t->accumulated_ms = 0;
        t->started_at_ms = platform_ticks_ms();
        t->running = false;
        return;
    }
}

void app_open_timer(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Timer", 0.28f, 0.32f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    TimerState *t = calloc(1, sizeof(TimerState));
    w->user_data = t;
    w->on_render = timer_render;
    w->on_click = timer_click;
}
