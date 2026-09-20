#include "shell.h"
#include "theme.h"
#include <string.h>

AppWindow *wm_find_window(ShellState *s, uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (s->windows[i].used && s->windows[i].id == id) return &s->windows[i];
    return NULL;
}

static AppWindow *free_slot(ShellState *s) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!s->windows[i].used) return &s->windows[i];
    return NULL;
}

AppWindow *wm_create_window(ShellState *s, const char *title, float w_ratio, float h_ratio) {
    AppWindow *w = free_slot(s);
    if (!w) return NULL;
    memset(w, 0, sizeof(*w));
    w->used = true;
    w->id = ++s->next_window_id;
    strncpy(w->title, title, MAX_TITLE - 1);

    int ww = (int)(s->sw * w_ratio);
    int wh = (int)(s->sh * h_ratio);
    w->rect.w = ww;
    w->rect.h = wh;
    w->rect.x = (s->sw - ww) / 2;
    w->rect.y = (s->sh - wh) / 2;
    w->is_max = false;
    w->minimized = false;

    s->focused_window_id = w->id;
    return w;
}

void wm_close_window(ShellState *s, AppWindow *w) {
    if (!w) return;
    if (s->focused_window_id == w->id) s->focused_window_id = 0;
    w->used = false;
}

static PRect titlebar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y, w->rect.w, TITLEBAR_H };
}

static void control_button_rects(AppWindow *w, PRect out[3]) {
    int bw = 30, bh = TITLEBAR_H;
    int right = w->rect.x + w->rect.w;
    out[2] = (PRect){ right - bw,     w->rect.y, bw, bh };
    out[1] = (PRect){ right - bw * 2, w->rect.y, bw, bh };
    out[0] = (PRect){ right - bw * 3, w->rect.y, bw, bh };
}

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void toggle_maximize(ShellState *s, AppWindow *w) {
    if (!w->is_max) {
        w->normal_rect = w->rect;
        w->is_max = true;
        w->rect = (PRect){ 0, 0, s->sw, s->sh };
    } else {
        w->is_max = false;
        w->rect = w->normal_rect;
    }
}

void wm_render_windows(ShellState *s) {
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_WINDOWS; i++) {
            AppWindow *w = &s->windows[i];
            if (!w->used || w->minimized) continue;
            bool focused = (w->id == s->focused_window_id);
            if ((pass == 0 && focused) || (pass == 1 && !focused)) continue;

            platform_fill_rect(w->rect, color_window_bg());

            PRect tbar = titlebar_rect(w);
            platform_fill_rect(tbar, color_title_bg());
            draw_text(w->title, tbar.x + 8, tbar.y + (TITLEBAR_H - platform_text_height()) / 2, color_text());

            PRect ctrls[3];
            control_button_rects(w, ctrls);
            const char *labels[3] = { "_", "[]", "X" };
            for (int b = 0; b < 3; b++) {
                PColor c = (b == 2) ? color_danger() : color_title_bg();
                platform_fill_rect(ctrls[b], c);
                draw_text_centered(labels[b], ctrls[b], color_text());
            }

            platform_draw_rect_outline(w->rect, color_title_bg());

            if (w->on_render) {
                w->on_render(w);
            } else {
                PRect content = { w->rect.x, tbar.y + TITLEBAR_H, w->rect.w, w->rect.h - TITLEBAR_H };
                draw_text_centered("( app not implemented )", content, color_text_dim());
            }
        }
    }
}

bool wm_handle_event(ShellState *s, PEvent *e) {
    for (int order = 0; order < 2; order++) {
        for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
            AppWindow *w = &s->windows[i];
            if (!w->used || w->minimized) continue;
            bool focused = (w->id == s->focused_window_id);
            if ((order == 0 && !focused) || (order == 1 && focused)) continue;

            PRect tbar = titlebar_rect(w);
            PRect ctrls[3];
            control_button_rects(w, ctrls);

            if (e->type == PEV_MOUSE_DOWN && e->button == PBTN_RIGHT) {
                if (point_in(w->rect, e->x, e->y)) {
                    s->focused_window_id = w->id;
                    if (w->on_rightclick)
                        w->on_rightclick(w, e->x - w->rect.x, e->y - w->rect.y);
                    return true;
                }
            } else if (e->type == PEV_MOUSE_DOWN && e->button == PBTN_LEFT) {
                if (point_in(ctrls[0], e->x, e->y)) { w->minimized = true; return true; }
                if (point_in(ctrls[1], e->x, e->y)) { toggle_maximize(s, w); return true; }
                if (point_in(ctrls[2], e->x, e->y)) { wm_close_window(s, w); return true; }
                if (point_in(tbar, e->x, e->y)) {
                    s->focused_window_id = w->id;
                    w->dragging = true;
                    w->drag_off_x = e->x - w->rect.x;
                    w->drag_off_y = e->y - w->rect.y;
                    return true;
                }
                if (point_in(w->rect, e->x, e->y)) {
                    s->focused_window_id = w->id;
                    if (w->on_click) w->on_click(w, e->x - w->rect.x, e->y - w->rect.y);
                    if (w->on_drag) w->content_dragging = true;
                    return true;
                }
            } else if (e->type == PEV_MOUSE_UP) {
                w->dragging = false;
                w->content_dragging = false;
            } else if (e->type == PEV_MOUSE_MOVE && w->dragging) {
                w->rect.x = e->x - w->drag_off_x;
                w->rect.y = e->y - w->drag_off_y;
                return true;
            } else if (e->type == PEV_MOUSE_MOVE && w->content_dragging && w->on_drag) {
                w->on_drag(w, e->x - w->rect.x, e->y - w->rect.y);
                return true;
            }
        }
    }
    return false;
}
