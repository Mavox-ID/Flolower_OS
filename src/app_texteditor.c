#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TE_MAX_BUF 262144

typedef struct {
    char path[1280];
    char filename[100];
    struct AppWindow *w;
    char *buf;
    int   len;
    int   cursor;
    int   scroll_line;
    bool  modified;
} TEState;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int te_line_start(TEState *t, int pos) {
    while (pos > 0 && t->buf[pos - 1] != '\n') pos--;
    return pos;
}
static int te_line_end(TEState *t, int pos) {
    while (pos < t->len && t->buf[pos] != '\n') pos++;
    return pos;
}
static int te_line_index(TEState *t, int pos) {
    int n = 0;
    for (int i = 0; i < pos && i < t->len; i++) if (t->buf[i] == '\n') n++;
    return n;
}
static int te_offset_of_line(TEState *t, int line_idx) {
    int n = 0, i = 0;
    while (i < t->len && n < line_idx) { if (t->buf[i] == '\n') n++; i++; }
    return i;
}

static void te_update_title(TEState *t) {
    if (!t->w) return;
    snprintf(t->w->title, MAX_TITLE, "%s%s", t->modified ? "* " : "", t->filename);
}

static void te_insert_text(TEState *t, const char *text) {
    size_t add = strlen(text);
    if ((size_t)t->len + add >= (size_t)TE_MAX_BUF - 1) return;
    memmove(t->buf + t->cursor + add, t->buf + t->cursor, t->len - t->cursor);
    memcpy(t->buf + t->cursor, text, add);
    t->len += (int)add;
    t->cursor += (int)add;
    t->modified = true;
    te_update_title(t);
}

static void te_backspace(TEState *t) {
    if (t->cursor <= 0) return;
    memmove(t->buf + t->cursor - 1, t->buf + t->cursor, t->len - t->cursor);
    t->len--; t->cursor--;
    t->modified = true;
    te_update_title(t);
}

static void te_delete_fwd(TEState *t) {
    if (t->cursor >= t->len) return;
    memmove(t->buf + t->cursor, t->buf + t->cursor + 1, t->len - t->cursor - 1);
    t->len--;
    t->modified = true;
    te_update_title(t);
}

static void te_move_up(TEState *t) {
    int ls = te_line_start(t, t->cursor);
    if (ls == 0) { t->cursor = 0; return; }
    int col = t->cursor - ls;
    int prev_end = ls - 1;
    int prev_start = te_line_start(t, prev_end);
    int prev_len = prev_end - prev_start;
    t->cursor = prev_start + (col < prev_len ? col : prev_len);
}
static void te_move_down(TEState *t) {
    int le = te_line_end(t, t->cursor);
    if (le >= t->len) { t->cursor = t->len; return; }
    int ls = te_line_start(t, t->cursor);
    int col = t->cursor - ls;
    int next_start = le + 1;
    int next_end = te_line_end(t, next_start);
    int next_len = next_end - next_start;
    t->cursor = next_start + (col < next_len ? col : next_len);
}

static void te_load(TEState *t, const char *path) {
    strncpy(t->path, path, sizeof(t->path) - 1);
    const char *base = strrchr(path, '/');
    strncpy(t->filename, base ? base + 1 : path, sizeof(t->filename) - 1);

    FILE *f = fopen(path, "rb");
    if (f) {
        t->len = (int)fread(t->buf, 1, TE_MAX_BUF - 1, f);
        fclose(f);
    } else {
        t->len = 0;
    }
    t->buf[t->len] = '\0';
    t->cursor = 0;
    t->scroll_line = 0;
    t->modified = false;
}

static void te_save(ShellState *s, TEState *t) {
    FILE *f = fopen(t->path, "wb");
    if (!f) { notify_show(s, "Could not save file", 2000); return; }
    fwrite(t->buf, 1, t->len, f);
    fclose(f);
    t->modified = false;
    te_update_title(t);
    notify_show(s, "Saved", 1200);
}

static PRect te_toolbar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, w->rect.w, 30 };
}
static PRect te_save_btn_rect(AppWindow *w) {
    PRect tb = te_toolbar_rect(w);
    return (PRect){ tb.x + tb.w - 84, tb.y + 3, 76, tb.h - 6 };
}
static PRect te_content_rect(AppWindow *w) {
    PRect tb = te_toolbar_rect(w);
    return (PRect){ w->rect.x + 6, tb.y + tb.h + 4, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - tb.h - 10 };
}

static ShellState *g_te_shell = NULL;

static void te_render(AppWindow *w) {
    TEState *t = (TEState *)w->user_data;
    int line_h = platform_text_height() + 2;

    PRect tb = te_toolbar_rect(w);
    platform_fill_rect(tb, color_menu_item());

    PRect save_btn = te_save_btn_rect(w);
    platform_fill_rect(save_btn, color_task_active());

    PRect content = te_content_rect(w);
    PColor cbg = { 0x0c, 0x0a, 0x12 };
    platform_fill_rect(content, cbg);

    draw_text(t->path, tb.x + 8, tb.y + (tb.h - platform_text_height()) / 2, color_text_dim());
    draw_text_centered("Save", save_btn, color_text());

    int cur_line = te_line_index(t, t->cursor);
    int visible = content.h / line_h;
    if (visible < 1) visible = 1;
    if (cur_line < t->scroll_line) t->scroll_line = cur_line;
    if (cur_line >= t->scroll_line + visible) t->scroll_line = cur_line - visible + 1;
    if (t->scroll_line < 0) t->scroll_line = 0;

    int off = te_offset_of_line(t, t->scroll_line);
    int y = content.y + 2;
    int line_idx = t->scroll_line;
    char line_buf[512];

    while (off <= t->len && (line_idx - t->scroll_line) < visible) {
        int end = te_line_end(t, off);
        int llen = end - off;
        if (llen >= (int)sizeof(line_buf)) llen = sizeof(line_buf) - 1;
        memcpy(line_buf, t->buf + off, llen);
        line_buf[llen] = '\0';

        if (line_buf[0]) draw_text(line_buf, content.x + 4, y, color_text());

        if (line_idx == cur_line) {
            int col = t->cursor - off;
            if (col < 0) col = 0;
            if (col > llen) col = llen;
            char pre[512];
            memcpy(pre, line_buf, col);
            pre[col] = '\0';
            int cx = content.x + 4 + platform_text_width(pre);
            PRect cursor_rect = { cx, y, 2, line_h - 2 };
            platform_fill_rect(cursor_rect, color_accent());
        }

        if (end >= t->len) break;
        off = end + 1;
        line_idx++;
        y += line_h;
    }
}

static void te_click(AppWindow *w, int lx, int ly) {
    TEState *t = (TEState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;
    int line_h = platform_text_height() + 2;

    PRect save_btn = te_save_btn_rect(w);
    if (point_in(save_btn, mx, my)) { te_save(g_te_shell, t); return; }

    PRect content = te_content_rect(w);
    if (!point_in(content, mx, my)) return;

    int line_idx = t->scroll_line + (my - content.y) / line_h;
    int off = te_offset_of_line(t, line_idx);
    int end = te_line_end(t, off);
    int col = 0;
    char probe[512];
    int llen = end - off;
    if (llen >= (int)sizeof(probe)) llen = sizeof(probe) - 1;
    memcpy(probe, t->buf + off, llen);
    probe[llen] = '\0';
    int target_x = mx - (content.x + 4);
    while (col < llen) {
        probe[col + 1] = '\0';
        if (platform_text_width(probe) > target_x) break;
        col++;
    }
    t->cursor = off + col;
}

static void te_textinput(AppWindow *w, const char *text) {
    te_insert_text((TEState *)w->user_data, text);
}

static void te_keydown(AppWindow *w, PKeycode key, int mods) {
    TEState *t = (TEState *)w->user_data;
    if (key == PK_BACKSPACE) te_backspace(t);
    else if (key == PK_DELETE) te_delete_fwd(t);
    else if (key == PK_RETURN) te_insert_text(t, "\n");
    else if (key == PK_LEFT) { if (t->cursor > 0) t->cursor--; }
    else if (key == PK_RIGHT) { if (t->cursor < t->len) t->cursor++; }
    else if (key == PK_UP) te_move_up(t);
    else if (key == PK_DOWN) te_move_down(t);
    else if (key == PK_S && (mods & PMOD_CTRL)) te_save(g_te_shell, t);
}

void app_open_text_editor(ShellState *s, const char *path) {
    g_te_shell = s;
    char title[MAX_TITLE];
    const char *base = strrchr(path, '/');
    snprintf(title, sizeof(title), "%s", base ? base + 1 : path);

    AppWindow *w = wm_create_window(s, title, 0.5f, 0.6f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    TEState *t = calloc(1, sizeof(TEState));
    t->buf = malloc(TE_MAX_BUF);
    t->w = w;
    te_load(t, path);

    w->user_data = t;
    w->on_render = te_render;
    w->on_click = te_click;
    w->on_textinput = te_textinput;
    w->on_keydown = te_keydown;
}
#endif
