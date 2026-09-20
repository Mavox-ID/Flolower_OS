#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#define TRASH_MAX_ENTRIES 256
#define TRASH_ROW_H 28

typedef struct { char name[256]; bool is_dir; } TrashEntry;

typedef struct {
    TrashEntry entries[TRASH_MAX_ENTRIES];
    int n_entries;
    int scroll;
} TrashState;

static ShellState *g_trash_shell = NULL;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void trash_reload(TrashState *t) {
    t->n_entries = 0;
    t->scroll = 0;
    DIR *d = opendir("/Trash");
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && t->n_entries < TRASH_MAX_ENTRIES) {
        if (de->d_name[0] == '.') continue;
        char full[600];
        snprintf(full, sizeof(full), "/Trash/%s", de->d_name);
        struct stat st;
        bool is_dir = (stat(full, &st) == 0) && S_ISDIR(st.st_mode);
        TrashEntry *e = &t->entries[t->n_entries++];
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        e->is_dir = is_dir;
    }
    closedir(d);
}

static PRect list_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 6, w->rect.y + TITLEBAR_H + 6, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - 12 };
}

static void trash_render(AppWindow *w) {
    TrashState *t = (TrashState *)w->user_data;
    PRect list = list_rect(w);
    PColor lbg = { 0x18, 0x12, 0x1f };
    platform_fill_rect(list, lbg);

    if (t->n_entries == 0) {
        draw_text("Trash is empty", list.x + 8, list.y + 8, color_text_dim());
        return;
    }

    int visible = list.h / TRASH_ROW_H;
    int start = t->scroll;
    int end = start + visible;
    if (end > t->n_entries) end = t->n_entries;

    for (int i = start; i < end; i++) {
        int row = i - start;
        PRect r = { list.x, list.y + row * TRASH_ROW_H, list.w, TRASH_ROW_H };

        PRect restore_btn = { r.x + r.w - 140, r.y + 3, 66, TRASH_ROW_H - 6 };
        PRect delete_btn  = { r.x + r.w - 68,  r.y + 3, 62, TRASH_ROW_H - 6 };

        const char *glyph = t->entries[i].is_dir ? "D" : "F";
        draw_text(glyph, r.x + 4, r.y + 4, color_text());
        draw_text(t->entries[i].name, r.x + 24, r.y + 4, color_text());

        platform_fill_rect(restore_btn, color_task_active());
        draw_text_centered("Restore", restore_btn, color_text());
        platform_fill_rect(delete_btn, color_danger());
        draw_text_centered("Delete", delete_btn, color_text());
    }
}

static void trash_click(AppWindow *w, int lx, int ly) {
    TrashState *t = (TrashState *)w->user_data;
    if (!g_trash_shell) return;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    PRect list = list_rect(w);
    if (!point_in(list, mx, my)) return;

    int visible = list.h / TRASH_ROW_H;
    (void)visible;
    int row = (my - list.y) / TRASH_ROW_H;
    int idx = t->scroll + row;
    if (idx < 0 || idx >= t->n_entries) return;

    PRect r = { list.x, list.y + row * TRASH_ROW_H, list.w, TRASH_ROW_H };
    PRect restore_btn = { r.x + r.w - 140, r.y + 3, 66, TRASH_ROW_H - 6 };
    PRect delete_btn  = { r.x + r.w - 68,  r.y + 3, 62, TRASH_ROW_H - 6 };

    char src[600];
    snprintf(src, sizeof(src), "/Trash/%s", t->entries[idx].name);

    if (point_in(restore_btn, mx, my)) {
        char dest[600];
        snprintf(dest, sizeof(dest), "/%s", t->entries[idx].name);
        bool ok = (rename(src, dest) == 0);
        char msg[300];
        snprintf(msg, sizeof(msg), ok ? "Restored %s to /" : "Could not restore %s", t->entries[idx].name);
        notify_show(g_trash_shell, msg, 2000);
        trash_reload(t);
        return;
    }
    if (point_in(delete_btn, mx, my)) {
        bool ok;
        if (t->entries[idx].is_dir) {
            char cmd[700];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", src);
            ok = (system(cmd) == 0);
        } else {
            ok = (remove(src) == 0);
        }
        char msg[300];
        snprintf(msg, sizeof(msg), ok ? "Permanently deleted %s" : "Could not delete %s", t->entries[idx].name);
        notify_show(g_trash_shell, msg, 2000);
        trash_reload(t);
        return;
    }
}

static void trash_wheel(AppWindow *w, int dy) {
    TrashState *t = (TrashState *)w->user_data;
    PRect list = list_rect(w);
    int visible = list.h / TRASH_ROW_H;
    t->scroll -= dy * 2;
    if (t->scroll < 0) t->scroll = 0;
    int max_scroll = t->n_entries - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (t->scroll > max_scroll) t->scroll = max_scroll;
}

void app_open_trash(ShellState *s) {
    g_trash_shell = s;
    AppWindow *w = wm_create_window(s, "Trash", 0.4f, 0.45f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    TrashState *t = calloc(1, sizeof(TrashState));
    trash_reload(t);

    w->user_data = t;
    w->on_render = trash_render;
    w->on_click = trash_click;
    w->on_wheel = trash_wheel;
}
#endif
