#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#ifdef HAVE_FS
#include <sys/stat.h>
#endif

typedef struct { const char *glyph; const char *title; const char *key; } AppDef;
static const AppDef DESKTOP_APPS[] = {
    { "#",  "Calculator",   "calculator"   },
    { "N",  "Notes",        "notes"        },
    { "C",  "Calendar",     "calendar"     },
    { ">_", "Terminal",     "terminal"     },
    { "T",  "Trash",        "trash"        },
    { "F",  "Files",        "files"        },
    { "G",  "Smile Game",   "smile_game"   },
    { "P",  "Paint",        "paint"        },
    { "@",  "Timer",        "timer"        },
    { "E",  "Text Editor",  "text_editor"  },
    { "S",  "Settings",     "settings"     },
    { "M",  "Task Manager", "task_manager" },
    { "B",  "Browser",      "browser"      },
};
#define N_DESKTOP_APPS (int)(sizeof(DESKTOP_APPS)/sizeof(DESKTOP_APPS[0]))

static uint32_t last_click_time = 0;
static char     last_click_key[64] = "";
#define DOUBLE_CLICK_MS 400

static DesktopIcon *free_icon_slot(ShellState *s) {
    for (int i = 0; i < MAX_ICONS; i++)
        if (!s->icons[i].used) return &s->icons[i];
    return NULL;
}

void desktop_add_icon(ShellState *s, const char *label, const char *title, const char *key, int x, int y) {
    DesktopIcon *ic = free_icon_slot(s);
    if (!ic) return;
    memset(ic, 0, sizeof(*ic));
    ic->used = true;
    strncpy(ic->emoji_label, label, sizeof(ic->emoji_label) - 1);
    strncpy(ic->title, title, sizeof(ic->title) - 1);
    strncpy(ic->key, key, sizeof(ic->key) - 1);
    ic->x = x;
    ic->y = y;
}

void desktop_load_icons(ShellState *s) {
    int per_column = (s->sh - TASKBAR_H - 24) / (ICON_H + 8);
    if (per_column < 1) per_column = 1;
    for (int i = 0; i < N_DESKTOP_APPS; i++) {
        int col = i / per_column;
        int row = i % per_column;
        int x = 24 + col * 108;
        int y = 24 + row * (ICON_H + 8);
        desktop_add_icon(s, DESKTOP_APPS[i].glyph, DESKTOP_APPS[i].title, DESKTOP_APPS[i].key, x, y);
    }
#ifdef HAVE_FS
    persist_load_icon_positions(s);
#endif
}

void desktop_reset_layout(ShellState *s) {
    for (int i = 0; i < MAX_ICONS; i++) s->icons[i].used = false;
    int per_column = (s->sh - TASKBAR_H - 24) / (ICON_H + 8);
    if (per_column < 1) per_column = 1;
    for (int i = 0; i < N_DESKTOP_APPS; i++) {
        int col = i / per_column;
        int row = i % per_column;
        int x = 24 + col * 108;
        int y = 24 + row * (ICON_H + 8);
        desktop_add_icon(s, DESKTOP_APPS[i].glyph, DESKTOP_APPS[i].title, DESKTOP_APPS[i].key, x, y);
    }
    desktop_save_icons(s);
}

void desktop_save_icons(ShellState *s) {
#ifdef HAVE_FS
    persist_save_icon_positions(s);
#else
    (void)s;
#endif
}

static PRect icon_rect(DesktopIcon *ic) {
    return (PRect){ ic->x, ic->y, ICON_W, ICON_H };
}

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void desktop_render(ShellState *s) {
    for (int i = 0; i < MAX_ICONS; i++) {
        DesktopIcon *ic = &s->icons[i];
        if (!ic->used) continue;
        PRect box = icon_rect(ic);

        PRect glyph_box = { box.x, box.y, ICON_W, 56 };
        platform_fill_rect(glyph_box, color_menu_item());
        draw_text_centered(ic->emoji_label, glyph_box, color_text());

        PRect label_box = { box.x, box.y + 60, ICON_W, ICON_H - 60 };
        draw_text_centered(ic->title, label_box, color_text());
    }
}

static void launch_icon(ShellState *s, DesktopIcon *ic) {
    if (strncmp(ic->key, "file_", 5) == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Opening %s", ic->key + 5);
        notify_show(s, msg, 2000);
        return;
    }
    app_launch(s, ic->key, ic->title);
}

static bool ctx_menu_open = false;
static int  ctx_menu_x = 0, ctx_menu_y = 0;

#ifdef HAVE_FS
static void create_item_on_disk(ShellState *s, const char *kind, int x, int y) {
    char name[128];
    const char *base;
    if (strcmp(kind, "folder") == 0)      base = "New Folder";
    else if (strcmp(kind, "text") == 0)   base = "New File.txt";
    else                                   base = "script.py";

    strncpy(name, base, sizeof(name) - 1);
    name[sizeof(name)-1] = '\0';
    int c = 1;
    struct stat st;
    while (stat(name, &st) == 0) {
        snprintf(name, sizeof(name), "%s (%d)", base, c++);
    }

    char key[160];
    const char *glyph;
    if (strcmp(kind, "folder") == 0) {
        mkdir(name, 0755);
        snprintf(key, sizeof(key), "folder_%s", name);
        glyph = "D";
    } else {
        FILE *f = fopen(name, "w");
        if (f) {
            if (strcmp(kind, "python") == 0)
                fprintf(f, "# New script\nprint(\"Hello!\")\n");
            fclose(f);
        }
        snprintf(key, sizeof(key), "file_%s", name);
        glyph = (strcmp(kind, "python") == 0) ? "PY" : "TXT";
    }
    desktop_add_icon(s, glyph, name, key, x, y);
    desktop_save_icons(s);

    char msg[160];
    snprintf(msg, sizeof(msg), "Created %s", name);
    notify_show(s, msg, 2000);
}
#endif

static void render_ctx_menu(ShellState *s) {
    if (!ctx_menu_open) return;
    const char *items[] = { "New folder", "New text file", "New Python script", "Refresh", "Settings" };
    int n = 5;
    int row_h = 28;
    PRect bg = { ctx_menu_x, ctx_menu_y, 200, n * row_h };
    platform_fill_rect(bg, color_menu_bg());
    for (int i = 0; i < n; i++) {
        PRect row = { ctx_menu_x, ctx_menu_y + i * row_h, 200, row_h };
        draw_text(items[i], row.x + 8, row.y + (row_h - platform_text_height()) / 2, color_text());
    }
    (void)s;
}

static bool ctx_menu_handle_click(ShellState *s, int mx, int my) {
    if (!ctx_menu_open) return false;
    int row_h = 28;
    PRect bg = { ctx_menu_x, ctx_menu_y, 200, 5 * row_h };
    if (!point_in(bg, mx, my)) { ctx_menu_open = false; return true; }
    int idx = (my - ctx_menu_y) / row_h;
    switch (idx) {
        case 0:
#ifdef HAVE_FS
            create_item_on_disk(s, "folder", ctx_menu_x, ctx_menu_y);
#else
            notify_show(s, "Needs Flolower FS driver", 2000);
#endif
            break;
        case 1:
#ifdef HAVE_FS
            create_item_on_disk(s, "text", ctx_menu_x, ctx_menu_y);
#else
            notify_show(s, "Needs Flolower FS driver", 2000);
#endif
            break;
        case 2:
#ifdef HAVE_FS
            create_item_on_disk(s, "python", ctx_menu_x, ctx_menu_y);
#else
            notify_show(s, "Needs Flolower FS driver", 2000);
#endif
            break;
        case 3: notify_show(s, "Desktop refreshed", 1500); break;
        case 4: app_launch(s, "settings", "Settings"); break;
    }
    ctx_menu_open = false;
    return true;
}

bool desktop_handle_event(ShellState *s, PEvent *e) {
    if (ctx_menu_open) {
        if (e->type == PEV_MOUSE_DOWN)
            return ctx_menu_handle_click(s, e->x, e->y);
        return false;
    }

    if (e->type == PEV_MOUSE_DOWN && e->button == PBTN_RIGHT) {
        ctx_menu_open = true;
        ctx_menu_x = e->x;
        ctx_menu_y = e->y;
        return true;
    }

    for (int i = MAX_ICONS - 1; i >= 0; i--) {
        DesktopIcon *ic = &s->icons[i];
        if (!ic->used) continue;
        PRect box = icon_rect(ic);

        if (e->type == PEV_MOUSE_DOWN && e->button == PBTN_LEFT) {
            if (point_in(box, e->x, e->y)) {
                uint32_t now = platform_ticks_ms();
                if (strcmp(last_click_key, ic->key) == 0 && now - last_click_time < DOUBLE_CLICK_MS) {
                    launch_icon(s, ic);
                    last_click_key[0] = '\0';
                } else {
                    last_click_time = now;
                    strncpy(last_click_key, ic->key, sizeof(last_click_key) - 1);
                }
                ic->dragging = true;
                ic->drag_off_x = e->x - ic->x;
                ic->drag_off_y = e->y - ic->y;
                return true;
            }
        } else if (e->type == PEV_MOUSE_UP) {
            if (ic->dragging) {
                ic->dragging = false;
                desktop_save_icons(s);
            }
        } else if (e->type == PEV_MOUSE_MOVE && ic->dragging) {
            int nx = e->x - ic->drag_off_x;
            int ny = e->y - ic->drag_off_y;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx > s->sw - ICON_W) nx = s->sw - ICON_W;
            if (ny > s->sh - TASKBAR_H - ICON_H) ny = s->sh - TASKBAR_H - ICON_H;
            ic->x = nx;
            ic->y = ny;
            return true;
        }
    }
    return false;
}

void desktop_render_overlay(ShellState *s) {
    render_ctx_menu(s);
}
