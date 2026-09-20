#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include <strings.h>

#define FILES_MAX_ENTRIES 512
#define FILES_ROW_H        26

typedef struct { char name[256]; bool is_dir; } FileEntry;

typedef struct {
    char cwd[1024];
    FileEntry entries[FILES_MAX_ENTRIES];
    int  n_entries;
    int  scroll;
    int  selected;

    int  renaming_index;
    char rename_buf[256];
    int  rename_len;
    bool renaming_is_new;
    bool renaming_is_dir;

    bool ctx_open;
    int  ctx_x, ctx_y;
    int  ctx_target_row;

    int  pending_delete_index;
} FilesState;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int entry_cmp(const void *a, const void *b) {
    const FileEntry *ea = a, *eb = b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcasecmp(ea->name, eb->name);
}

static void files_reload(FilesState *fs) {
    fs->n_entries = 0;
    fs->scroll = 0;
    fs->selected = -1;
    fs->ctx_open = false;
    fs->pending_delete_index = -1;
    fs->renaming_index = -1;

    DIR *d = opendir(fs->cwd);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && fs->n_entries < FILES_MAX_ENTRIES) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (de->d_name[0] == '.') continue;

        char full[1280];
        snprintf(full, sizeof(full), "%s/%s", fs->cwd, de->d_name);
        struct stat st;
        bool is_dir = (stat(full, &st) == 0) && S_ISDIR(st.st_mode);

        FileEntry *fe = &fs->entries[fs->n_entries++];
        size_t namelen = strlen(de->d_name);
        if (namelen >= sizeof(fe->name)) namelen = sizeof(fe->name) - 1;
        memcpy(fe->name, de->d_name, namelen);
        fe->name[namelen] = '\0';
        fe->is_dir = is_dir;
    }
    closedir(d);
    qsort(fs->entries, fs->n_entries, sizeof(FileEntry), entry_cmp);
}

static bool at_root(const char *path) { return strcmp(path, "/") == 0; }

static void files_go_up(FilesState *fs) {
    if (at_root(fs->cwd)) return;
    char copy[1024];
    strncpy(copy, fs->cwd, sizeof(copy) - 1);
    copy[sizeof(copy)-1] = '\0';
    char *parent = dirname(copy);
    if (strcmp(parent, ".") == 0) return;
    strncpy(fs->cwd, parent, sizeof(fs->cwd) - 1);
    fs->cwd[sizeof(fs->cwd)-1] = '\0';
    files_reload(fs);
}

static void files_enter(FilesState *fs, const char *name) {
    char next[1280];
    if (at_root(fs->cwd)) snprintf(next, sizeof(next), "/%s", name);
    else snprintf(next, sizeof(next), "%s/%s", fs->cwd, name);
    strncpy(fs->cwd, next, sizeof(fs->cwd) - 1);
    fs->cwd[sizeof(fs->cwd)-1] = '\0';
    files_reload(fs);
}

static bool files_looks_like_text(const char *full) {
    FILE *f = fopen(full, "rb");
    if (!f) return false;
    unsigned char buf[512];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    for (size_t i = 0; i < n; i++) if (buf[i] == 0) return false;
    return true;
}

static void files_open_external(ShellState *s, FilesState *fs, const char *name) {
    char full[1280];
    snprintf(full, sizeof(full), "%s/%s", fs->cwd, name);
    char cmd[1400];
#if defined(__APPLE__)
    snprintf(cmd, sizeof(cmd), "open \"%s\" &", full);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1 &", full);
#endif
    if (system(cmd) != 0) {  }
    char msg[300];
    snprintf(msg, sizeof(msg), "Opening %s", name);
    notify_show(s, msg, 1800);
}

static bool name_exists(FilesState *fs, const char *name) {
    for (int i = 0; i < fs->n_entries; i++)
        if (strcmp(fs->entries[i].name, name) == 0) return true;
    return false;
}

static void unique_name(FilesState *fs, const char *base, char *out, size_t outsz) {
    strncpy(out, base, outsz - 1);
    out[outsz - 1] = '\0';
    int n = 2;
    while (name_exists(fs, out)) {
        const char *dot = strrchr(base, '.');
        if (dot && dot != base) snprintf(out, outsz, "%.*s (%d)%s", (int)(dot - base), base, n, dot);
        else snprintf(out, outsz, "%s (%d)", base, n);
        n++;
    }
}

static int find_index_by_name(FilesState *fs, const char *name) {
    for (int i = 0; i < fs->n_entries; i++)
        if (strcmp(fs->entries[i].name, name) == 0) return i;
    return -1;
}

static void files_create_folder(FilesState *fs) {
    char name[256];
    unique_name(fs, "New Folder", name, sizeof(name));
    char full[1280];
    snprintf(full, sizeof(full), "%s/%s", fs->cwd, name);
    mkdir(full, 0755);
    files_reload(fs);
    int idx = find_index_by_name(fs, name);
    if (idx >= 0) {
        fs->renaming_index = idx;
        fs->renaming_is_new = true;
        fs->renaming_is_dir = true;
        snprintf(fs->rename_buf, sizeof(fs->rename_buf), "%s", name);
        fs->rename_len = (int)strlen(fs->rename_buf);
        fs->selected = idx;
    }
}

static void files_create_file(FilesState *fs) {
    char name[256];
    unique_name(fs, "New File.txt", name, sizeof(name));
    char full[1280];
    snprintf(full, sizeof(full), "%s/%s", fs->cwd, name);
    FILE *f = fopen(full, "w");
    if (f) fclose(f);
    files_reload(fs);
    int idx = find_index_by_name(fs, name);
    if (idx >= 0) {
        fs->renaming_index = idx;
        fs->renaming_is_new = true;
        fs->renaming_is_dir = false;
        snprintf(fs->rename_buf, sizeof(fs->rename_buf), "%s", name);
        fs->rename_len = (int)strlen(fs->rename_buf);
        fs->selected = idx;
    }
}

static void files_delete_confirmed(ShellState *s, FilesState *fs, int idx) {
    if (idx < 0 || idx >= fs->n_entries) return;
    char src[1280];
    snprintf(src, sizeof(src), "%s/%s", fs->cwd, fs->entries[idx].name);

    struct stat st;
    if (stat("/Trash", &st) != 0) mkdir("/Trash", 0755);

    char dest_name[256];
    snprintf(dest_name, sizeof(dest_name), "%s", fs->entries[idx].name);
    char dest[1300];
    snprintf(dest, sizeof(dest), "/Trash/%s", dest_name);
    int n = 2;
    while (stat(dest, &st) == 0) {
        const char *orig = fs->entries[idx].name;
        const char *dot = strrchr(orig, '.');
        if (dot && dot != orig) snprintf(dest_name, sizeof(dest_name), "%.*s (%d)%s", (int)(dot - orig), orig, n, dot);
        else snprintf(dest_name, sizeof(dest_name), "%s (%d)", orig, n);
        snprintf(dest, sizeof(dest), "/Trash/%s", dest_name);
        n++;
    }

    bool ok = (rename(src, dest) == 0);
    char msg[300];
    snprintf(msg, sizeof(msg), ok ? "Moved %s to Trash" : "Could not delete %s", fs->entries[idx].name);
    notify_show(s, msg, 2000);
    fs->pending_delete_index = -1;
    files_reload(fs);
}

static void files_begin_rename(FilesState *fs, int idx) {
    if (idx < 0 || idx >= fs->n_entries) return;
    fs->renaming_index = idx;
    fs->renaming_is_new = false;
    fs->renaming_is_dir = fs->entries[idx].is_dir;
    snprintf(fs->rename_buf, sizeof(fs->rename_buf), "%s", fs->entries[idx].name);
    fs->rename_len = (int)strlen(fs->rename_buf);
}

static void files_commit_rename(ShellState *s, FilesState *fs) {
    if (fs->renaming_index < 0) return;
    const char *old_name = fs->entries[fs->renaming_index].name;
    if (fs->rename_len == 0 || strcmp(fs->rename_buf, old_name) == 0) { fs->renaming_index = -1; return; }
    char old_full[1280], new_full[1280];
    snprintf(old_full, sizeof(old_full), "%s/%s", fs->cwd, old_name);
    snprintf(new_full, sizeof(new_full), "%s/%s", fs->cwd, fs->rename_buf);
    if (rename(old_full, new_full) == 0) notify_show(s, "Renamed", 1200);
    else notify_show(s, "Rename failed", 1800);
    fs->renaming_index = -1;
    files_reload(fs);
}

static PRect files_pathbar_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 4, w->rect.y + TITLEBAR_H + 4, w->rect.w - 8, 24 };
}
static PRect files_up_btn_rect(AppWindow *w) {
    PRect pb = files_pathbar_rect(w);
    return (PRect){ pb.x, pb.y, 34, pb.h };
}
static PRect files_list_rect(AppWindow *w) {
    PRect pb = files_pathbar_rect(w);
    return (PRect){ w->rect.x + 4, pb.y + pb.h + 6, w->rect.w - 8,
                     w->rect.h - (pb.y - w->rect.y) - pb.h - 12 };
}
static int files_visible_rows(AppWindow *w) { return files_list_rect(w).h / FILES_ROW_H; }

static void files_render(AppWindow *w) {
    FilesState *fs = (FilesState *)w->user_data;

    PRect pb = files_pathbar_rect(w);
    platform_fill_rect(pb, color_menu_item());

    PRect up = files_up_btn_rect(w);
    platform_fill_rect(up, color_task_active());

    PRect list = files_list_rect(w);
    PColor lbg = { 0x18, 0x12, 0x1f };
    platform_fill_rect(list, lbg);

    draw_text_centered("..", up, color_text());
    draw_text(fs->cwd, pb.x + up.w + 8, pb.y + (pb.h - platform_text_height()) / 2, color_text());

    int visible = files_visible_rows(w);
    int start = fs->scroll;
    int end = start + visible;
    if (end > fs->n_entries) end = fs->n_entries;

    for (int i = start; i < end; i++) {
        PRect row = { list.x, list.y + (i - start) * FILES_ROW_H, list.w, FILES_ROW_H };
        if (i == fs->selected) platform_fill_rect(row, color_task_active());

        const char *glyph = fs->entries[i].is_dir ? "D" : "F";
        draw_text(glyph, row.x + 4, row.y + 4, color_text());

        if (i == fs->renaming_index) {
            PRect editbox = { row.x + 24, row.y + 2, row.w - 30, FILES_ROW_H - 4 };
            platform_fill_rect(editbox, color_menu_bg());
            platform_draw_rect_outline(editbox, color_accent());
            char shown[300];
            snprintf(shown, sizeof(shown), "%s_", fs->rename_buf);
            draw_text(shown, editbox.x + 4, editbox.y + 4, color_text());
        } else {
            draw_text(fs->entries[i].name, row.x + 26, row.y + 4, color_text());
        }

        if (i == fs->pending_delete_index) {
            PRect confirm = { row.x, row.y, row.w, FILES_ROW_H };
            platform_fill_rect(confirm, color_danger());
            char q[300];
            snprintf(q, sizeof(q), "Delete '%s'?  [Delete]   [Cancel]", fs->entries[i].name);
            draw_text(q, confirm.x + 4, confirm.y + 4, color_text());
        }
    }

    if (fs->n_entries == 0) draw_text("(empty folder)", list.x + 8, list.y + 8, color_text_dim());

    if (fs->ctx_open) {
        const char *items[2];
        int n = 2;
        if (fs->ctx_target_row >= 0) { items[0] = "Rename"; items[1] = "Delete"; }
        else { items[0] = "New folder"; items[1] = "New file"; }
        PRect menu = { fs->ctx_x, fs->ctx_y, 160, n * 26 };
        platform_fill_rect(menu, color_menu_bg());
        for (int i = 0; i < n; i++) {
            PRect mrow = { menu.x, menu.y + i * 26, menu.w, 26 };
            draw_text(items[i], mrow.x + 8, mrow.y + 5, color_text());
        }
    }
}

static uint32_t files_last_click_time = 0;
static int      files_last_click_idx = -1;

static ShellState *g_files_shell = NULL;

static void files_ctx_click(FilesState *fs, int mx, int my) {
    int n = 2;
    PRect menu = { fs->ctx_x, fs->ctx_y, 160, n * 26 };
    if (point_in(menu, mx, my)) {
        int item = (my - menu.y) / 26;
        if (fs->ctx_target_row >= 0) {
            if (item == 0) files_begin_rename(fs, fs->ctx_target_row);
            else if (item == 1) fs->pending_delete_index = fs->ctx_target_row;
        } else {
            if (item == 0) files_create_folder(fs);
            else if (item == 1) files_create_file(fs);
        }
    }
    fs->ctx_open = false;
}

static void files_click_ex(AppWindow *w, int lx, int ly) {
    FilesState *fs = (FilesState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;
    PRect list = files_list_rect(w);

    if (fs->ctx_open) { files_ctx_click(fs, mx, my); return; }

    if (fs->renaming_index >= 0) {
        files_commit_rename(g_files_shell, fs);
        return;
    }

    if (fs->pending_delete_index >= 0) {
        int row_on_screen = fs->pending_delete_index - fs->scroll;
        PRect row = { list.x, list.y + row_on_screen * FILES_ROW_H, list.w, FILES_ROW_H };
        if (point_in(row, mx, my)) {
            bool hit_delete = (mx - row.x) < row.w / 2;
            if (hit_delete) files_delete_confirmed(g_files_shell, fs, fs->pending_delete_index);
            else fs->pending_delete_index = -1;
        } else {
            fs->pending_delete_index = -1;
        }
        return;
    }

    PRect up = files_up_btn_rect(w);
    if (point_in(up, mx, my)) { files_go_up(fs); return; }
    if (!point_in(list, mx, my)) return;

    int row = (my - list.y) / FILES_ROW_H + fs->scroll;
    if (row < 0 || row >= fs->n_entries) return;

    uint32_t now = platform_ticks_ms();
    bool dbl = (row == files_last_click_idx && now - files_last_click_time < 400);
    files_last_click_time = now;
    files_last_click_idx = row;
    fs->selected = row;

    if (dbl) {
        if (fs->entries[row].is_dir) {
            files_enter(fs, fs->entries[row].name);
        } else {
            char full[1280];
            snprintf(full, sizeof(full), "%s/%s", fs->cwd, fs->entries[row].name);
            if (files_looks_like_text(full)) app_open_text_editor(g_files_shell, full);
            else files_open_external(g_files_shell, fs, fs->entries[row].name);
        }
    }
}

static void files_rightclick(AppWindow *w, int lx, int ly) {
    FilesState *fs = (FilesState *)w->user_data;
    if (fs->renaming_index >= 0) return;
    int mx = w->rect.x + lx, my = w->rect.y + ly;
    PRect list = files_list_rect(w);
    if (!point_in(list, mx, my)) return;

    int row = (my - list.y) / FILES_ROW_H + fs->scroll;
    fs->ctx_target_row = (row >= 0 && row < fs->n_entries) ? row : -1;
    fs->ctx_open = true;
    fs->ctx_x = mx;
    fs->ctx_y = my;
    fs->pending_delete_index = -1;
}

static void files_textinput(AppWindow *w, const char *text) {
    FilesState *fs = (FilesState *)w->user_data;
    if (fs->renaming_index < 0) return;
    size_t add = strlen(text);
    if ((size_t)fs->rename_len + add < sizeof(fs->rename_buf) - 1) {
        memcpy(fs->rename_buf + fs->rename_len, text, add);
        fs->rename_len += (int)add;
        fs->rename_buf[fs->rename_len] = '\0';
    }
}

static void files_keydown(AppWindow *w, PKeycode key, int mods) {
    (void)mods;
    FilesState *fs = (FilesState *)w->user_data;
    if (fs->renaming_index < 0) return;
    if (key == PK_BACKSPACE) {
        if (fs->rename_len > 0) { fs->rename_len--; fs->rename_buf[fs->rename_len] = '\0'; }
    } else if (key == PK_RETURN) {
        files_commit_rename(g_files_shell, fs);
    }
}

static void files_wheel(AppWindow *w, int dy) {
    FilesState *fs = (FilesState *)w->user_data;
    int visible = files_visible_rows(w);
    fs->scroll -= dy * 2;
    if (fs->scroll < 0) fs->scroll = 0;
    int max_scroll = fs->n_entries - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (fs->scroll > max_scroll) fs->scroll = max_scroll;
}

void app_open_files(ShellState *s) {
    g_files_shell = s;
    AppWindow *w = wm_create_window(s, "Files", 0.42f, 0.5f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    FilesState *fs = calloc(1, sizeof(FilesState));
    fs->renaming_index = -1;
    fs->pending_delete_index = -1;
    const char *home = getenv("HOME");
    strncpy(fs->cwd, home ? home : "/", sizeof(fs->cwd) - 1);
    files_reload(fs);

    w->user_data = fs;
    w->on_render = files_render;
    w->on_click = files_click_ex;
    w->on_rightclick = files_rightclick;
    w->on_textinput = files_textinput;
    w->on_keydown = files_keydown;
    w->on_wheel = files_wheel;
}
#endif
