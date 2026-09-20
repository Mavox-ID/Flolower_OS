#ifdef HAVE_FS
#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BROWSER_MAX_CONTENT 8192
#define BROWSER_MAX_LINKS   32
#define BROWSER_MAX_HISTORY 16
#define BROWSER_LINE_LEN    200

typedef struct {
    char target[64];
    PRect rect;
} BrowserLink;

typedef struct {
    char content[BROWSER_MAX_CONTENT];
    char current_page[64];
    char history[BROWSER_MAX_HISTORY][64];
    int  history_len;
    int  scroll;
    BrowserLink links[BROWSER_MAX_LINKS];
    int  n_links;
} BrowserState;

static ShellState *g_browser_shell = NULL;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static bool fetch_page(const char *name, char *out, int out_size) {
#ifndef PLATFORM_HOSTED
    extern bool repo_find(const char *name, void *out_entry);
    extern int  repo_fetch(const void *entry, char *glyph_out, char *content_out, int content_out_size);
    unsigned char entry_buf[40];
    char glyph[4];
    if (!repo_find(name, entry_buf)) return false;
    int n = repo_fetch(entry_buf, glyph, out, out_size);
    return n >= 0;
#else
    char path[300];
    snprintf(path, sizeof(path), "repo/%s.txt", name);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    int n = (int)fread(out, 1, (size_t)(out_size - 1), f);
    fclose(f);
    out[n] = '\0';
    return true;
#endif
}

static void browser_load(BrowserState *b, const char *name, bool push_history) {
    if (!fetch_page(name, b->content, sizeof(b->content))) {
        snprintf(b->content, sizeof(b->content),
                 "Page '%s' not found in the repo.\n\n[[home]]", name);
    }
    snprintf(b->current_page, sizeof(b->current_page), "%s", name);
    b->scroll = 0;
    if (push_history) {
        if (b->history_len >= BROWSER_MAX_HISTORY) {
            memmove(b->history[0], b->history[1], (size_t)(BROWSER_MAX_HISTORY - 1) * sizeof(b->history[0]));
            b->history_len--;
        }
        snprintf(b->history[b->history_len], sizeof(b->history[0]), "%s", name);
        b->history_len++;
    }
}

static PRect toolbar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, w->rect.w, 30 };
}
static PRect back_btn_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ tb.x + 4, tb.y + 3, 66, tb.h - 6 };
}
static PRect content_rect(AppWindow *w) {
    PRect tb = toolbar_rect(w);
    return (PRect){ w->rect.x + 6, tb.y + tb.h + 4, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - tb.h - 10 };
}

static void browser_render(AppWindow *w) {
    BrowserState *b = (BrowserState *)w->user_data;

    PRect tb = toolbar_rect(w);
    platform_fill_rect(tb, color_menu_item());
    PRect back = back_btn_rect(w);
    platform_fill_rect(back, b->history_len > 1 ? color_task_active() : color_title_bg());
    draw_text_centered("< Back", back, color_text());
    draw_text(b->current_page, back.x + back.w + 10, tb.y + (tb.h - platform_text_height()) / 2, color_text_dim());

    PRect content = content_rect(w);
    PColor cbg = { 0x0c, 0x0a, 0x12 };
    platform_fill_rect(content, cbg);

    b->n_links = 0;
    int line_h = platform_text_height() + 4;
    int y = content.y + 2 - b->scroll * line_h;
    const char *p = b->content;

    while (*p) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > BROWSER_LINE_LEN) len = BROWSER_LINE_LEN;
        char line[BROWSER_LINE_LEN + 1];
        memcpy(line, p, (size_t)len);
        line[len] = '\0';

        if (y >= content.y - line_h && y < content.y + content.h) {
            int x = content.x + 4;
            char *seg = line;
            while (*seg) {
                char *lb = strstr(seg, "[[");
                if (!lb) {
                    draw_text(seg, x, y, color_text());
                    break;
                }
                *lb = '\0';
                draw_text(seg, x, y, color_text());
                x += platform_text_width(seg);

                char *rb = strstr(lb + 2, "]]");
                if (!rb) {
                    draw_text(lb + 2, x, y, color_accent());
                    break;
                }
                *rb = '\0';
                char *target = lb + 2;
                int lw = platform_text_width(target);
                if (b->n_links < BROWSER_MAX_LINKS) {
                    PRect r = { x, y, lw, line_h };
                    snprintf(b->links[b->n_links].target, sizeof(b->links[0].target), "%s", target);
                    b->links[b->n_links].rect = r;
                    b->n_links++;
                }
                draw_text(target, x, y, color_accent());
                x += lw;
                seg = rb + 2;
            }
        }

        y += line_h;
        if (!nl) break;
        p = nl + 1;
    }
}

static void browser_click(AppWindow *w, int lx, int ly) {
    BrowserState *b = (BrowserState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    PRect back = back_btn_rect(w);
    if (point_in(back, mx, my)) {
        if (b->history_len > 1) {
            b->history_len--;
            char prev[64];
            snprintf(prev, sizeof(prev), "%s", b->history[b->history_len - 1]);
            b->history_len--;
            browser_load(b, prev, true);
        }
        return;
    }

    for (int i = 0; i < b->n_links; i++) {
        if (point_in(b->links[i].rect, mx, my)) {
            browser_load(b, b->links[i].target, true);
            return;
        }
    }
}

static void browser_wheel(AppWindow *w, int dy) {
    BrowserState *b = (BrowserState *)w->user_data;
    b->scroll -= dy * 2;
    if (b->scroll < 0) b->scroll = 0;
}

void app_open_browser(ShellState *s) {
    g_browser_shell = s;
    AppWindow *w = wm_create_window(s, "Browser", 0.5f, 0.55f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    BrowserState *b = calloc(1, sizeof(BrowserState));
    browser_load(b, "home", true);

    w->user_data = b;
    w->on_render = browser_render;
    w->on_click = browser_click;
    w->on_wheel = browser_wheel;
    (void)g_browser_shell;
}
#endif
