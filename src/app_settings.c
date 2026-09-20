#include "shell.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int active_tab;
} SettingsState;

static ShellState *g_settings_shell = NULL;

static const PColor BG_SWATCHES[] = {
    { 0x0f, 0x0b, 0x18 },
    { 0x0a, 0x14, 0x18 },
    { 0x18, 0x0a, 0x0a },
    { 0x0a, 0x0a, 0x0a },
    { 0x10, 0x18, 0x0a },
    { 0x14, 0x0a, 0x18 },
};
static const PColor ACCENT_SWATCHES[] = {
    { 0x4d, 0x7f, 0xff },
    { 0xff, 0x4d, 0x7f },
    { 0x4d, 0xd9, 0x6b },
    { 0xff, 0xd4, 0x4d },
    { 0xff, 0x8a, 0x4d },
    { 0xb0, 0x4d, 0xff },
};
#define N_SWATCH 6

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
static bool color_eq(PColor a, PColor b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static PRect sidebar_rect(AppWindow *w) {
    return (PRect){ w->rect.x, w->rect.y + TITLEBAR_H, 100, w->rect.h - TITLEBAR_H };
}
static PRect tab_rect(AppWindow *w, int i) {
    PRect sb = sidebar_rect(w);
    return (PRect){ sb.x, sb.y + i * 40, sb.w, 40 };
}
static PRect content_rect(AppWindow *w) {
    PRect sb = sidebar_rect(w);
    return (PRect){ sb.x + sb.w, w->rect.y + TITLEBAR_H, w->rect.w - sb.w, w->rect.h - TITLEBAR_H };
}

static PRect bg_swatch_rect(PRect content, int i) {
    return (PRect){ content.x + 16 + i * 44, content.y + 50, 34, 34 };
}
static PRect accent_swatch_rect(PRect content, int i) {
    return (PRect){ content.x + 16 + i * 44, content.y + 130, 34, 34 };
}
static PRect save_btn_rect(PRect content)  { return (PRect){ content.x + 16, content.y + 190, 90, 30 }; }
static PRect resetc_btn_rect(PRect content){ return (PRect){ content.x + 114, content.y + 190, 120, 30 }; }

static PRect beep_low_btn_rect(PRect content)  { return (PRect){ content.x + 16, content.y + 50, 120, 34 }; }
static PRect beep_high_btn_rect(PRect content) { return (PRect){ content.x + 146, content.y + 50, 120, 34 }; }

static PRect restart_btn_rect(PRect content)     { return (PRect){ content.x + 16, content.y + content.h - 96, 120, 32 }; }
static PRect reset_layout_btn_rect(PRect content){ return (PRect){ content.x + 16, content.y + content.h - 52, 160, 32 }; }

static void render_appearance(AppWindow *w, PRect content) {
    (void)w;
    draw_text("Background", content.x + 16, content.y + 24, color_text());
    for (int i = 0; i < N_SWATCH; i++) {
        PRect sw = bg_swatch_rect(content, i);
        platform_fill_rect(sw, BG_SWATCHES[i]);
        if (color_eq(g_color_bg, BG_SWATCHES[i])) platform_draw_rect_outline(sw, color_accent());
    }

    draw_text("Accent", content.x + 16, content.y + 104, color_text());
    for (int i = 0; i < N_SWATCH; i++) {
        PRect sw = accent_swatch_rect(content, i);
        platform_fill_rect(sw, ACCENT_SWATCHES[i]);
        if (color_eq(g_color_accent, ACCENT_SWATCHES[i])) platform_draw_rect_outline(sw, color_text());
    }

    PRect save_btn = save_btn_rect(content);
    PRect reset_btn = resetc_btn_rect(content);
    platform_fill_rect(save_btn, color_task_active());
    draw_text_centered("Save", save_btn, color_text());
    platform_fill_rect(reset_btn, color_menu_item());
    draw_text_centered("Reset colors", reset_btn, color_text());

#ifndef HAVE_FS
    draw_text("(no filesystem: colors reset on restart)", content.x + 16, content.y + 234, color_text_dim());
#endif
}

static void render_sound(AppWindow *w, PRect content) {
    (void)w;
    draw_text("Test tone", content.x + 16, content.y + 24, color_text());
    PRect low = beep_low_btn_rect(content);
    PRect high = beep_high_btn_rect(content);
    platform_fill_rect(low, color_task_active());
    draw_text_centered("Low beep", low, color_text());
    platform_fill_rect(high, color_task_active());
    draw_text_centered("High beep", high, color_text());
#ifdef PLATFORM_HOSTED
    draw_text("(SDL audio)", content.x + 16, content.y + 100, color_text_dim());
#else
    draw_text("(PC speaker, PIT channel 2)", content.x + 16, content.y + 100, color_text_dim());
#endif
}

static void render_system(AppWindow *w, PRect content) {
    (void)w;
    char line[96];
    int line_h = platform_text_height() + 8;
    int y = content.y + 20;

    draw_text("Flolower OS", content.x + 16, y, color_text()); y += line_h;

    snprintf(line, sizeof(line), "Resolution: %d x %d", platform_width(), platform_height());
    draw_text(line, content.x + 16, y, color_text_dim()); y += line_h;

#ifdef PLATFORM_HOSTED
    draw_text("Platform: hosted (SDL2)", content.x + 16, y, color_text_dim()); y += line_h;
#else
    draw_text("Platform: bare metal", content.x + 16, y, color_text_dim()); y += line_h;
#endif

    uint32_t sec = platform_ticks_ms() / 1000;
    snprintf(line, sizeof(line), "Uptime: %um %us", sec / 60, sec % 60);
    draw_text(line, content.x + 16, y, color_text_dim()); y += line_h;

#ifdef HAVE_FS
    draw_text("Filesystem: available", content.x + 16, y, color_text_dim()); y += line_h;
#else
    draw_text("Filesystem: none", content.x + 16, y, color_text_dim()); y += line_h;
#endif

    PRect restart_btn = restart_btn_rect(content);
    platform_fill_rect(restart_btn, color_danger());
    draw_text_centered("Restart", restart_btn, color_text());

    PRect reset_layout_btn = reset_layout_btn_rect(content);
    platform_fill_rect(reset_layout_btn, color_menu_item());
    draw_text_centered("Reset desktop layout", reset_layout_btn, color_text());
}

static void settings_render(AppWindow *w) {
    SettingsState *st = (SettingsState *)w->user_data;

    PRect sb = sidebar_rect(w);
    platform_fill_rect(sb, color_menu_bg());
    const char *labels[3] = { "Appearance", "Sound", "System" };
    for (int i = 0; i < 3; i++) {
        PRect t = tab_rect(w, i);
        if (i == st->active_tab) platform_fill_rect(t, color_task_active());
        draw_text(labels[i], t.x + 8, t.y + (t.h - platform_text_height()) / 2, color_text());
    }

    PRect content = content_rect(w);
    platform_fill_rect(content, (PColor){ 0x0c, 0x0a, 0x12 });

    if (st->active_tab == 0) render_appearance(w, content);
    else if (st->active_tab == 1) render_sound(w, content);
    else render_system(w, content);
}

static void settings_click(AppWindow *w, int lx, int ly) {
    SettingsState *st = (SettingsState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    for (int i = 0; i < 3; i++) {
        if (point_in(tab_rect(w, i), mx, my)) { st->active_tab = i; return; }
    }

    PRect content = content_rect(w);

    if (st->active_tab == 0) {
        for (int i = 0; i < N_SWATCH; i++) {
            if (point_in(bg_swatch_rect(content, i), mx, my)) { theme_set_bg(BG_SWATCHES[i]); return; }
            if (point_in(accent_swatch_rect(content, i), mx, my)) { theme_set_accent(ACCENT_SWATCHES[i]); return; }
        }
        if (point_in(save_btn_rect(content), mx, my)) {
#ifdef HAVE_FS
            theme_save_settings();
            if (g_settings_shell) notify_show(g_settings_shell, "Colors saved", 1500);
#else
            if (g_settings_shell) notify_show(g_settings_shell, "No filesystem to save to", 1800);
#endif
            return;
        }
        if (point_in(resetc_btn_rect(content), mx, my)) { theme_reset_defaults(); return; }
    } else if (st->active_tab == 1) {
        if (point_in(beep_low_btn_rect(content), mx, my)) { platform_beep(220, 200); return; }
        if (point_in(beep_high_btn_rect(content), mx, my)) { platform_beep(880, 200); return; }
    } else {
        if (point_in(restart_btn_rect(content), mx, my)) {
#ifdef PLATFORM_HOSTED
            if (g_settings_shell) notify_show(g_settings_shell, "Restart isn't available in the hosted build", 2500);
#else
            extern void system_restart(void);
            system_restart();
#endif
            return;
        }
        if (point_in(reset_layout_btn_rect(content), mx, my)) {
            if (g_settings_shell) {
                desktop_reset_layout(g_settings_shell);
                notify_show(g_settings_shell, "Desktop layout reset", 1500);
            }
            return;
        }
    }
}

void app_open_settings(ShellState *s) {
    g_settings_shell = s;
    AppWindow *w = wm_create_window(s, "Settings", 0.42f, 0.5f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    SettingsState *st = calloc(1, sizeof(SettingsState));
    w->user_data = st;
    w->on_render = settings_render;
    w->on_click = settings_click;
}
