#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NOTES_MAX_BUF 65536
#define NOTES_PATH    "/notes.txt"

typedef struct {
    char *buf;
    int   len;
    int   cursor;
    int   scroll_line;
    bool  dirty;
    uint32_t last_edit_ms;
} NotesState;

static int notes_line_start(NotesState *n, int pos) {
    while (pos > 0 && n->buf[pos - 1] != '\n') pos--;
    return pos;
}
static int notes_line_end(NotesState *n, int pos) {
    while (pos < n->len && n->buf[pos] != '\n') pos++;
    return pos;
}
static int notes_line_index(NotesState *n, int pos) {
    int c = 0;
    for (int i = 0; i < pos && i < n->len; i++) if (n->buf[i] == '\n') c++;
    return c;
}
static int notes_offset_of_line(NotesState *n, int line_idx) {
    int c = 0, i = 0;
    while (i < n->len && c < line_idx) { if (n->buf[i] == '\n') c++; i++; }
    return i;
}

static void notes_save(NotesState *n) {
    FILE *f = fopen(NOTES_PATH, "wb");
    if (!f) return;
    fwrite(n->buf, 1, (size_t)n->len, f);
    fclose(f);
    n->dirty = false;
}

static void notes_load(NotesState *n) {
    FILE *f = fopen(NOTES_PATH, "rb");
    if (f) {
        n->len = (int)fread(n->buf, 1, NOTES_MAX_BUF - 1, f);
        fclose(f);
    } else {
        n->len = 0;
    }
    n->buf[n->len] = '\0';
    n->cursor = 0;
    n->scroll_line = 0;
    n->dirty = false;
}

static void notes_insert(NotesState *n, const char *text) {
    size_t add = strlen(text);
    if ((size_t)n->len + add >= (size_t)NOTES_MAX_BUF - 1) return;
    memmove(n->buf + n->cursor + add, n->buf + n->cursor, (size_t)(n->len - n->cursor));
    memcpy(n->buf + n->cursor, text, add);
    n->len += (int)add;
    n->cursor += (int)add;
    n->dirty = true;
}
static void notes_backspace(NotesState *n) {
    if (n->cursor <= 0) return;
    memmove(n->buf + n->cursor - 1, n->buf + n->cursor, (size_t)(n->len - n->cursor));
    n->len--; n->cursor--;
    n->dirty = true;
}
static void notes_delete_fwd(NotesState *n) {
    if (n->cursor >= n->len) return;
    memmove(n->buf + n->cursor, n->buf + n->cursor + 1, (size_t)(n->len - n->cursor - 1));
    n->len--;
    n->dirty = true;
}
static void notes_move_up(NotesState *n) {
    int ls = notes_line_start(n, n->cursor);
    if (ls == 0) { n->cursor = 0; return; }
    int col = n->cursor - ls;
    int prev_end = ls - 1;
    int prev_start = notes_line_start(n, prev_end);
    int prev_len = prev_end - prev_start;
    n->cursor = prev_start + (col < prev_len ? col : prev_len);
}
static void notes_move_down(NotesState *n) {
    int le = notes_line_end(n, n->cursor);
    if (le >= n->len) { n->cursor = n->len; return; }
    int ls = notes_line_start(n, n->cursor);
    int col = n->cursor - ls;
    int next_start = le + 1;
    int next_end = notes_line_end(n, next_start);
    int next_len = next_end - next_start;
    n->cursor = next_start + (col < next_len ? col : next_len);
}

static PRect toolbar_rect(AppWindow *w) { return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, w->rect.w, 26 }; }
static PRect content_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ w->rect.x + 6, tb.y + tb.h + 4, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - tb.h - 10 };
}

static void notes_render(AppWindow *w) {
    NotesState *n = (NotesState *)w->user_data;
    int line_h = platform_text_height() + 2;

    PRect tb = toolbar_rect(w);
    platform_fill_rect(tb, color_menu_item());
    draw_text(n->dirty ? "Notes - saving..." : "Notes - saved", tb.x + 8, tb.y + (tb.h - platform_text_height()) / 2, color_text_dim());

    PRect content = content_rect(w);
    PColor cbg = { 0x0c, 0x0a, 0x12 };
    platform_fill_rect(content, cbg);

    int cur_line = notes_line_index(n, n->cursor);
    int visible = content.h / line_h;
    if (visible < 1) visible = 1;
    if (cur_line < n->scroll_line) n->scroll_line = cur_line;
    if (cur_line >= n->scroll_line + visible) n->scroll_line = cur_line - visible + 1;
    if (n->scroll_line < 0) n->scroll_line = 0;

    int off = notes_offset_of_line(n, n->scroll_line);
    int y = content.y + 2;
    int line_idx = n->scroll_line;
    char line_buf[512];

    while (off <= n->len && (line_idx - n->scroll_line) < visible) {
        int end = notes_line_end(n, off);
        int llen = end - off;
        if (llen >= (int)sizeof(line_buf)) llen = sizeof(line_buf) - 1;
        memcpy(line_buf, n->buf + off, (size_t)llen);
        line_buf[llen] = '\0';

        if (line_buf[0]) draw_text(line_buf, content.x + 4, y, color_text());

        if (line_idx == cur_line) {
            int col = n->cursor - off;
            if (col < 0) col = 0;
            if (col > llen) col = llen;
            char pre[512];
            memcpy(pre, line_buf, (size_t)col);
            pre[col] = '\0';
            int cx = content.x + 4 + platform_text_width(pre);
            PRect cursor_rect = { cx, y, 2, line_h - 2 };
            platform_fill_rect(cursor_rect, color_accent());
        }

        if (end >= n->len) break;
        off = end + 1;
        line_idx++;
        y += line_h;
    }
}

static void notes_click(AppWindow *w, int lx, int ly) {
    NotesState *n = (NotesState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;
    int line_h = platform_text_height() + 2;

    PRect content = content_rect(w);
    if (mx < content.x || mx >= content.x + content.w || my < content.y || my >= content.y + content.h) return;

    int line_idx = n->scroll_line + (my - content.y) / line_h;
    int off = notes_offset_of_line(n, line_idx);
    int end = notes_line_end(n, off);
    int col = 0;
    char probe[512];
    int llen = end - off;
    if (llen >= (int)sizeof(probe)) llen = sizeof(probe) - 1;
    memcpy(probe, n->buf + off, (size_t)llen);
    probe[llen] = '\0';
    int target_x = mx - (content.x + 4);
    while (col < llen) {
        probe[col + 1] = '\0';
        if (platform_text_width(probe) > target_x) break;
        col++;
    }
    n->cursor = off + col;
}

static void notes_textinput(AppWindow *w, const char *text) {
    notes_insert((NotesState *)w->user_data, text);
}

static void notes_keydown(AppWindow *w, PKeycode key, int mods) {
    (void)mods;
    NotesState *n = (NotesState *)w->user_data;
    if (key == PK_BACKSPACE) notes_backspace(n);
    else if (key == PK_DELETE) notes_delete_fwd(n);
    else if (key == PK_RETURN) notes_insert(n, "\n");
    else if (key == PK_LEFT) { if (n->cursor > 0) n->cursor--; }
    else if (key == PK_RIGHT) { if (n->cursor < n->len) n->cursor++; }
    else if (key == PK_UP) notes_move_up(n);
    else if (key == PK_DOWN) notes_move_down(n);

    if (n->dirty) notes_save(n);
}

void app_open_notes(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Notes", 0.35f, 0.45f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    NotesState *n = calloc(1, sizeof(NotesState));
    n->buf = malloc(NOTES_MAX_BUF);
    notes_load(n);

    w->user_data = n;
    w->on_render = notes_render;
    w->on_click = notes_click;
    w->on_textinput = notes_textinput;
    w->on_keydown = notes_keydown;
}
#endif
