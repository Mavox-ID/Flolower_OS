#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define TERM_MAX_LINES 400
#define TERM_LINE_LEN  200
#define TERM_INPUT_LEN 256

typedef struct {
    char lines[TERM_MAX_LINES][TERM_LINE_LEN];
    int  n_lines;
    int  scroll;
    char input[TERM_INPUT_LEN];
    int  input_len;
    char cwd[512];

    bool     clock_timer_active;
    uint32_t clock_started_ms;
    int      clock_timer_line_idx;

    int      clock_time_line_idx;
} TermState;

static ShellState *g_term_shell = NULL;
static int downloaded_app_count = 0;

static void term_add_line(TermState *t, const char *text) {
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= TERM_LINE_LEN) len = TERM_LINE_LEN - 1;

        if (t->n_lines < TERM_MAX_LINES) {
            memcpy(t->lines[t->n_lines], p, len);
            t->lines[t->n_lines][len] = '\0';
            t->n_lines++;
        } else {
            memmove(t->lines[0], t->lines[1], (TERM_MAX_LINES - 1) * TERM_LINE_LEN);
            memcpy(t->lines[TERM_MAX_LINES - 1], p, len);
            t->lines[TERM_MAX_LINES - 1][len] = '\0';
        }
        if (!nl) break;
        p = nl + 1;
    }
    t->scroll = 0;
}

static bool term_have_real_clock(void) {
#ifdef PLATFORM_HOSTED
    return true;
#else
    return false;
#endif
}

static void term_run_command(TermState *t, const char *cmd) {
    char prompt_line[512 + TERM_INPUT_LEN + 8];
    snprintf(prompt_line, sizeof(prompt_line), "%s $ %s", t->cwd, cmd);
    term_add_line(t, prompt_line);

    if (cmd[0] == '\0') return;
    if (strcmp(cmd, "clear") == 0) { t->n_lines = 0; return; }

    if (strncmp(cmd, "cd", 2) == 0 && (cmd[2] == '\0' || cmd[2] == ' ')) {
        const char *arg = cmd[2] == ' ' ? cmd + 3 : getenv("HOME");
        while (arg && *arg == ' ') arg++;
        if (!arg || *arg == '\0') arg = getenv("HOME");
        if (arg && chdir(arg) == 0) {
            if (getcwd(t->cwd, sizeof(t->cwd)) == NULL) {  }
        } else {
            term_add_line(t, "cd: no such directory");
        }
        return;
    }

    if (strncmp(cmd, "settime ", 8) == 0) {
        const char *arg = cmd + 8;
        while (*arg == ' ') arg++;
        int hh = 0, mm = 0, ss = 0;
        int n = sscanf(arg, "%d:%d:%d", &hh, &mm, &ss);
        if (n < 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
            term_add_line(t, "usage: settime HH:MM[:SS]  (0-23:0-59[:0-59])");
            return;
        }
        if (g_term_shell) {
            g_term_shell->virtual_base_sec = (uint32_t)(hh * 3600 + mm * 60 + ss);
            g_term_shell->virtual_base_ticks_ms = platform_ticks_ms();
            g_term_shell->virtual_time_set = true;
        }
        char msg[96];
        snprintf(msg, sizeof(msg), "Time set to %02d:%02d:%02d. Type 'clock' to see it tick (also shown in the taskbar).", hh, mm, ss);
        term_add_line(t, msg);
        return;
    }

    if (strcmp(cmd, "settime reset") == 0) {
        if (g_term_shell) g_term_shell->virtual_time_set = false;
        term_add_line(t, "Time reset to system clock.");
        return;
    }

    if (strcmp(cmd, "clock") == 0) {
        char buf[128];
        if (g_term_shell && g_term_shell->virtual_time_set) {
            uint32_t elapsed_sec = (platform_ticks_ms() - g_term_shell->virtual_base_ticks_ms) / 1000;
            uint32_t sec = (g_term_shell->virtual_base_sec + elapsed_sec) % 86400u;
            snprintf(buf, sizeof(buf), "Time (custom): %02u:%02u:%02u", sec / 3600, (sec % 3600) / 60, sec % 60);
            term_add_line(t, buf);
            t->clock_time_line_idx = t->n_lines - 1;
        } else if (term_have_real_clock()) {
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            if (lt) {
                strftime(buf, sizeof(buf), "Date: %d.%m.%Y   Time: %H:%M:%S", lt);
            } else {
                snprintf(buf, sizeof(buf), "Could not read system time");
            }
            term_add_line(t, buf);
            t->clock_time_line_idx = t->n_lines - 1;
        } else {
            term_add_line(t, "Date/time unavailable on bare metal (no RTC driver) - try 'settime HH:MM'.");
            t->clock_time_line_idx = -1;
        }

        t->clock_timer_active = true;
        t->clock_started_ms = platform_ticks_ms();
        term_add_line(t, "Timer: 00:00:00");
        t->clock_timer_line_idx = t->n_lines - 1;
        return;
    }

    if (strcmp(cmd, "clock stop") == 0) {
        if (t->clock_timer_active) {
            t->clock_timer_active = false;
            term_add_line(t, "Timer stopped.");
        } else {
            term_add_line(t, "Timer wasn't running.");
        }
        return;
    }

    if (strncmp(cmd, "download ", 9) == 0) {
        const char *name = cmd + 9;
        while (*name == ' ') name++;
        if (*name == '\0') {
            term_add_line(t, "usage: download <name>");
            return;
        }

        char glyph[4] = { 0 };
        char content[8192];
        bool got_real_content = false;

#ifndef PLATFORM_HOSTED
        extern bool repo_find(const char *name, void *out_entry);
        extern int  repo_fetch(const void *entry, char *glyph_out, char *content_out, int content_out_size);
        unsigned char entry_buf[40];
        if (repo_find(name, entry_buf)) {
            int n = repo_fetch(entry_buf, glyph, content, sizeof(content));
            if (n >= 0) got_real_content = true;
        }
#endif

        char path[600];
        if (strcmp(t->cwd, "/") == 0) snprintf(path, sizeof(path), "/%s.txt", name);
        else snprintf(path, sizeof(path), "%s/%s.txt", t->cwd, name);

        FILE *f = fopen(path, "w");
        if (f) {
            if (got_real_content) {
                fwrite(content, 1, strlen(content), f);
            } else {
                fprintf(f, "'%s' isn't in the app repo (try: cheatsheet, poem, apps),\n"
                           "or this is the hosted build, which has no on-disk repo to read.\n", name);
            }
            fclose(f);
        }

        if (glyph[0] == '\0') glyph[0] = (char)(name[0] >= 'a' && name[0] <= 'z' ? name[0] - 32 : name[0]);
        int col = downloaded_app_count / 6;
        int row = downloaded_app_count % 6;
        int x = 24 + (3 + col) * 108;
        int y = 24 + row * (ICON_H + 8);
        char key[64];
        snprintf(key, sizeof(key), "dl_%s", name);
        if (g_term_shell) {
            desktop_add_icon(g_term_shell, glyph, name, key, x, y);
            desktop_save_icons(g_term_shell);
        }
        downloaded_app_count++;

        char msg[300];
        if (got_real_content) snprintf(msg, sizeof(msg), "Downloaded '%s' from the repo (%d bytes) - icon added to desktop", name, (int)strlen(content));
        else snprintf(msg, sizeof(msg), "'%s' not found in repo - added a placeholder icon instead", name);
        term_add_line(t, msg);
        return;
    }

    char full[TERM_INPUT_LEN + 16];
    snprintf(full, sizeof(full), "%s 2>&1", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) { term_add_line(t, "error: could not run command"); return; }
    char buf[TERM_LINE_LEN];
    int shown = 0;
    while (fgets(buf, sizeof(buf), fp) && shown < 500) {
        size_t l = strlen(buf);
        if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
        term_add_line(t, buf);
        shown++;
    }
    pclose(fp);
}

static int term_line_height(void) { return platform_text_height() + 2; }

static PRect term_content_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 6, w->rect.y + TITLEBAR_H + 4,
                     w->rect.w - 12, w->rect.h - TITLEBAR_H - 34 };
}
static PRect term_input_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 6, w->rect.y + w->rect.h - 28, w->rect.w - 12, 24 };
}

static void term_render(AppWindow *w) {
    TermState *t = (TermState *)w->user_data;

    if (t->clock_time_line_idx >= 0 && t->clock_time_line_idx < t->n_lines) {
        char buf[128];
        buf[0] = '\0';
        if (g_term_shell && g_term_shell->virtual_time_set) {
            uint32_t elapsed_sec = (platform_ticks_ms() - g_term_shell->virtual_base_ticks_ms) / 1000;
            uint32_t sec = (g_term_shell->virtual_base_sec + elapsed_sec) % 86400u;
            snprintf(buf, sizeof(buf), "Time (custom): %02u:%02u:%02u", sec / 3600, (sec % 3600) / 60, sec % 60);
        } else if (term_have_real_clock()) {
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            if (lt) strftime(buf, sizeof(buf), "Date: %d.%m.%Y   Time: %H:%M:%S", lt);
        }
        if (buf[0]) snprintf(t->lines[t->clock_time_line_idx], TERM_LINE_LEN, "%s", buf);
    }

    if (t->clock_timer_active &&
        t->clock_timer_line_idx >= 0 &&
        t->clock_timer_line_idx < t->n_lines) {
        uint32_t elapsed_ms = platform_ticks_ms() - t->clock_started_ms;
        uint32_t total_sec = elapsed_ms / 1000;
        uint32_t hh = total_sec / 3600;
        uint32_t mm = (total_sec % 3600) / 60;
        uint32_t ss = total_sec % 60;
        snprintf(t->lines[t->clock_timer_line_idx], TERM_LINE_LEN,
                 "Timer: %02u:%02u:%02u", hh, mm, ss);
    }

    PRect content = term_content_rect(w);
    PColor cbg = { 0x0c, 0x0a, 0x12 };
    platform_fill_rect(content, cbg);

    PRect input_box = term_input_rect(w);
    platform_fill_rect(input_box, color_menu_item());

    int lh = term_line_height();
    int visible = content.h / lh;
    int start = t->n_lines - visible - t->scroll;
    if (start < 0) start = 0;
    int end = start + visible;
    if (end > t->n_lines) end = t->n_lines;

    int char_w = platform_text_width("A");
    if (char_w <= 0) char_w = 8;
    int max_chars = (content.w - 8) / char_w;
    if (max_chars < 1) max_chars = 1;

    int y = content.y + 2;
    int bottom = content.y + content.h;
    for (int i = start; i < end && y + lh <= bottom; i++) {
        const char *line = t->lines[i];
        size_t len = strlen(line);
        if (len == 0) { y += lh; continue; }
        size_t pos = 0;
        while (pos < len && y + lh <= bottom) {
            size_t chunk = len - pos;
            if (chunk > (size_t)max_chars) chunk = (size_t)max_chars;
            char wrapped[TERM_LINE_LEN];
            memcpy(wrapped, line + pos, chunk);
            wrapped[chunk] = '\0';
            draw_text(wrapped, content.x + 4, y, color_text());
            y += lh;
            pos += chunk;
        }
    }

    char shown_input[512 + TERM_INPUT_LEN + 8];
    snprintf(shown_input, sizeof(shown_input), "%s $ %s_", t->cwd, t->input);

    int input_max_chars = (input_box.w - 8) / char_w;
    if (input_max_chars < 1) input_max_chars = 1;
    size_t input_len = strlen(shown_input);
    const char *visible_input = shown_input;
    if (input_len > (size_t)input_max_chars) {
        visible_input = shown_input + (input_len - (size_t)input_max_chars);
    }
    draw_text(visible_input, input_box.x + 4, input_box.y + 4, color_text());
}

static void term_click(AppWindow *w, int lx, int ly) { (void)w; (void)lx; (void)ly; }

static void term_textinput(AppWindow *w, const char *text) {
    TermState *t = (TermState *)w->user_data;
    size_t add = strlen(text);
    if (t->input_len + add < TERM_INPUT_LEN - 1) {
        memcpy(t->input + t->input_len, text, add);
        t->input_len += (int)add;
        t->input[t->input_len] = '\0';
    }
}

static void term_keydown(AppWindow *w, PKeycode key, int mods) {
    (void)mods;
    TermState *t = (TermState *)w->user_data;
    if (key == PK_BACKSPACE) {
        if (t->input_len > 0) { t->input_len--; t->input[t->input_len] = '\0'; }
    } else if (key == PK_RETURN) {
        term_run_command(t, t->input);
        t->input_len = 0;
        t->input[0] = '\0';
    }
}

static void term_wheel(AppWindow *w, int dy) {
    TermState *t = (TermState *)w->user_data;
    t->scroll += dy * 2;
    if (t->scroll < 0) t->scroll = 0;
    if (t->scroll > t->n_lines) t->scroll = t->n_lines;
}

void app_open_terminal(ShellState *s) {
    g_term_shell = s;
    AppWindow *w = wm_create_window(s, "Terminal", 0.5f, 0.5f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    TermState *t = calloc(1, sizeof(TermState));
    if (getcwd(t->cwd, sizeof(t->cwd)) == NULL) strcpy(t->cwd, "~");
    t->clock_time_line_idx = -1;
    term_add_line(t, "Flolower Terminal - type a command and press Enter.");
    term_add_line(t, "Builtins: cd, clear, ls, pwd, cat, echo, mkdir, rm, touch, download <name>, clock, settime HH:MM[:SS].");

    w->user_data = t;
    w->on_render = term_render;
    w->on_click = term_click;
    w->on_textinput = term_textinput;
    w->on_keydown = term_keydown;
    w->on_wheel = term_wheel;
}
#endif
