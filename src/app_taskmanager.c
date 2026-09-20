#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ROW_H 30

typedef struct {
    int scroll;
} TaskManagerState;

static ShellState *g_tm_shell = NULL;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static PRect list_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 6, w->rect.y + TITLEBAR_H + 6, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - 12 };
}

static int count_open_windows(ShellState *s) {
    int n = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) if (s->windows[i].used) n++;
    return n;
}

static int nth_open_window(ShellState *s, int n) {
    int count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!s->windows[i].used) continue;
        if (count == n) return i;
        count++;
    }
    return -1;
}

static void taskmanager_render(AppWindow *w) {
    TaskManagerState *tm = (TaskManagerState *)w->user_data;
    if (!g_tm_shell) return;
    ShellState *s = g_tm_shell;

    PRect list = list_rect(w);
    PColor lbg = { 0x18, 0x12, 0x1f };
    platform_fill_rect(list, lbg);

    int visible = list.h / ROW_H;
    int total = count_open_windows(s);
    int start = tm->scroll;
    if (start > total - visible) start = total - visible;
    if (start < 0) start = 0;
    tm->scroll = start;

    for (int row = 0; row < visible; row++) {
        int idx = nth_open_window(s, start + row);
        if (idx < 0) break;
        AppWindow *ow = &s->windows[idx];

        PRect r = { list.x, list.y + row * ROW_H, list.w, ROW_H };
        if (ow->id == s->focused_window_id) platform_fill_rect(r, color_task_active());

        PRect close_btn = { r.x + r.w - 60, r.y + 4, 50, ROW_H - 8 };
        PRect focus_btn = { r.x + r.w - 120, r.y + 4, 54, ROW_H - 8 };

        char label[MAX_TITLE + 16];
        snprintf(label, sizeof(label), "%s%s", ow->title, ow->minimized ? " (minimized)" : "");
        draw_text(label, r.x + 8, r.y + (ROW_H - platform_text_height()) / 2, color_text());

        platform_fill_rect(focus_btn, color_menu_item());
        draw_text_centered("Focus", focus_btn, color_text());
        platform_fill_rect(close_btn, color_danger());
        draw_text_centered("Close", close_btn, color_text());
    }

    if (total == 0) {
        draw_text("(no open windows)", list.x + 8, list.y + 8, color_text_dim());
    }
}

static void taskmanager_click(AppWindow *w, int lx, int ly) {
    TaskManagerState *tm = (TaskManagerState *)w->user_data;
    if (!g_tm_shell) return;
    ShellState *s = g_tm_shell;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    PRect list = list_rect(w);
    if (!point_in(list, mx, my)) return;

    int visible = list.h / ROW_H;
    (void)visible;
    int row = (my - list.y) / ROW_H;
    int idx = nth_open_window(s, tm->scroll + row);
    if (idx < 0) return;
    AppWindow *ow = &s->windows[idx];

    PRect r = { list.x, list.y + row * ROW_H, list.w, ROW_H };
    PRect close_btn = { r.x + r.w - 60, r.y + 4, 50, ROW_H - 8 };
    PRect focus_btn = { r.x + r.w - 120, r.y + 4, 54, ROW_H - 8 };

    if (point_in(close_btn, mx, my)) {
        wm_close_window(s, ow);
        return;
    }
    if (point_in(focus_btn, mx, my)) {
        ow->minimized = false;
        s->focused_window_id = ow->id;
        return;
    }
}

static void taskmanager_wheel(AppWindow *w, int dy) {
    TaskManagerState *tm = (TaskManagerState *)w->user_data;
    tm->scroll -= dy * 2;
    if (tm->scroll < 0) tm->scroll = 0;
}

void app_open_taskmanager(ShellState *s) {
    g_tm_shell = s;
    AppWindow *w = wm_create_window(s, "Task Manager", 0.45f, 0.4f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    TaskManagerState *tm = calloc(1, sizeof(TaskManagerState));
    w->user_data = tm;
    w->on_render = taskmanager_render;
    w->on_click = taskmanager_click;
    w->on_wheel = taskmanager_wheel;
}
