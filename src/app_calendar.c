#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int month;
    int year;
} CalendarState;

static const char *MONTH_NAMES[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *WEEKDAY_NAMES[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int days_in_month(int m, int y) {
    static const int dm[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return dm[m - 1];
}

static int first_weekday(int month, int year) {
    int m = month, y = year, d = 1;
    if (m < 3) { m += 12; y -= 1; }
    int k = y % 100;
    int j = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7;
}

static PRect toolbar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, w->rect.w, 34 };
}
static PRect prev_btn_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ tb.x + 6, tb.y + 4, 30, tb.h - 8 };
}
static PRect next_btn_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ tb.x + tb.w - 36, tb.y + 4, 30, tb.h - 8 };
}
static PRect grid_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ w->rect.x + 6, tb.y + tb.h + 30, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - tb.h - 40 };
}

static void calendar_render(AppWindow *w) {
    CalendarState *c = (CalendarState *)w->user_data;

    PRect tb = toolbar_rect(w);
    platform_fill_rect(tb, color_menu_item());

    PRect prev = prev_btn_rect(w);
    PRect next = next_btn_rect(w);
    platform_fill_rect(prev, color_task_active());
    platform_fill_rect(next, color_task_active());
    draw_text_centered("<", prev, color_text());
    draw_text_centered(">", next, color_text());

    char title[64];
    snprintf(title, sizeof(title), "%s %d", MONTH_NAMES[c->month - 1], c->year);
    PRect title_box = { prev.x + prev.w, tb.y, next.x - (prev.x + prev.w), tb.h };
    draw_text_centered(title, title_box, color_text());

    PRect grid = grid_rect(w);
    int col_w = grid.w / 7;
    int header_y = tb.y + tb.h + 4;
    for (int i = 0; i < 7; i++) {
        PRect cell = { grid.x + i * col_w, header_y, col_w, 20 };
        draw_text_centered(WEEKDAY_NAMES[i], cell, color_text_dim());
    }

    int first_wd = first_weekday(c->month, c->year);
    int ndays = days_in_month(c->month, c->year);
    int row_h = grid.h / 6;

    for (int day = 1; day <= ndays; day++) {
        int cell_index = first_wd + (day - 1);
        int row = cell_index / 7;
        int col = cell_index % 7;
        PRect cell = { grid.x + col * col_w, grid.y + row * row_h, col_w, row_h };
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", day);
        draw_text(buf, cell.x + 4, cell.y + 2, color_text());
    }
}

static void calendar_click(AppWindow *w, int lx, int ly) {
    CalendarState *c = (CalendarState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    if (point_in(prev_btn_rect(w), mx, my)) {
        c->month--;
        if (c->month < 1) { c->month = 12; c->year--; }
        return;
    }
    if (point_in(next_btn_rect(w), mx, my)) {
        c->month++;
        if (c->month > 12) { c->month = 1; c->year++; }
        return;
    }
}

void app_open_calendar(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Calendar", 0.4f, 0.45f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    CalendarState *c = calloc(1, sizeof(CalendarState));
    c->month = 7;
    c->year = 2026;

    w->user_data = c;
    w->on_render = calendar_render;
    w->on_click = calendar_click;
}
