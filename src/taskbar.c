#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#ifdef PLATFORM_HOSTED
#include <time.h>
#endif

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static PRect start_button_rect(ShellState *s) {
    return (PRect){ 0, s->sh - TASKBAR_H, 110, TASKBAR_H };
}

void taskbar_render(ShellState *s) {
    PRect bar = { 0, s->sh - TASKBAR_H, s->sw, TASKBAR_H };
    platform_fill_rect(bar, color_taskbar_bg());

    PRect start = start_button_rect(s);
    platform_fill_rect(start, s->start_menu_open ? color_task_active() : color_menu_item());
    draw_text_centered("Flolower", start, color_text());

    int x = start.x + start.w + 8;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        AppWindow *w = &s->windows[i];
        if (!w->used) continue;
        PRect btn = { x, bar.y + 6, 160, TASKBAR_H - 12 };
        PColor c = (w->id == s->focused_window_id && !w->minimized) ? color_task_active() : color_menu_item();
        platform_fill_rect(btn, c);
        draw_text(w->title, btn.x + 8, btn.y + (btn.h - platform_text_height()) / 2, color_text());
        x += btn.w + 6;
    }

    uint32_t now = platform_ticks_ms();
    if (now - s->clock_last_update > 1000 || s->clock_text[0] == '\0') {
        if (s->virtual_time_set) {
            uint32_t elapsed_sec = (now - s->virtual_base_ticks_ms) / 1000;
            uint32_t sec = (s->virtual_base_sec + elapsed_sec) % 86400u;
            snprintf(s->clock_text, sizeof(s->clock_text), "%02u:%02u:%02u",
                     sec / 3600, (sec % 3600) / 60, sec % 60);
        } else {
#ifdef PLATFORM_HOSTED
            time_t t = time(NULL);
            struct tm *lt = localtime(&t);
            strftime(s->clock_text, sizeof(s->clock_text), "%H:%M:%S", lt);
#else
            uint32_t secs = now / 1000;
            snprintf(s->clock_text, sizeof(s->clock_text), "%02u:%02u:%02u",
                     (secs / 3600) % 24, (secs / 60) % 60, secs % 60);
#endif
        }
        s->clock_last_update = now;
    }
    PRect clock_box = { s->sw - 110, bar.y, 100, TASKBAR_H };
    draw_text_centered(s->clock_text, clock_box, color_text());
}

bool taskbar_handle_event(ShellState *s, PEvent *e) {
    if (e->type != PEV_MOUSE_DOWN || e->button != PBTN_LEFT) return false;

    PRect start = start_button_rect(s);
    if (point_in(start, e->x, e->y)) {
        startmenu_toggle(s, -1);
        return true;
    }

    PRect bar = { 0, s->sh - TASKBAR_H, s->sw, TASKBAR_H };
    if (!point_in(bar, e->x, e->y)) return false;

    int x = start.x + start.w + 8;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        AppWindow *w = &s->windows[i];
        if (!w->used) continue;
        PRect btn = { x, bar.y + 6, 160, TASKBAR_H - 12 };
        if (point_in(btn, e->x, e->y)) {
            if (w->minimized) {
                w->minimized = false;
                s->focused_window_id = w->id;
            } else if (s->focused_window_id == w->id) {
                w->minimized = true;
            } else {
                s->focused_window_id = w->id;
            }
            return true;
        }
        x += btn.w + 6;
    }
    return true;
}
