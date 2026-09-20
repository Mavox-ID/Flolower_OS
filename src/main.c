#include "shell.h"
#include "theme.h"
#include <string.h>

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static PRect quit_dialog_rect(ShellState *s) {
    int w = 320, h = 130;
    return (PRect){ (s->sw - w) / 2, (s->sh - h) / 2, w, h };
}

static void render_quit_dialog(ShellState *s) {
    if (!s->quit_dialog_open) return;

    PRect box = quit_dialog_rect(s);
    platform_fill_rect(box, color_window_bg());
    platform_draw_rect_outline(box, color_title_bg());

    PRect msg = { box.x, box.y + 18, box.w, 30 };
    draw_text_centered("Are you sure you want to quit?", msg, color_text());

    PRect quit_btn   = { box.x + 30,  box.y + 80, 110, 36 };
    PRect cancel_btn = { box.x + 180, box.y + 80, 110, 36 };
    platform_fill_rect(quit_btn, color_danger());
    platform_fill_rect(cancel_btn, color_menu_item());
    draw_text_centered("Quit", quit_btn, color_text());
    draw_text_centered("Cancel", cancel_btn, color_text());
}

static bool handle_quit_dialog_click(ShellState *s, int mx, int my) {
    PRect box = quit_dialog_rect(s);
    PRect quit_btn   = { box.x + 30,  box.y + 80, 110, 36 };
    PRect cancel_btn = { box.x + 180, box.y + 80, 110, 36 };
    if (point_in(quit_btn, mx, my)) {
        desktop_save_icons(s);
        s->running = false;
        return true;
    }
    if (point_in(cancel_btn, mx, my)) {
        s->quit_dialog_open = false;
        return true;
    }
    return true;
}

static void render_cursor(ShellState *s) {
    static const int widths[12]  = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 4, 4 };
    static const int offsets[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 3, 4 };
    const PColor black = { 0, 0, 0 };

    int cx = s->mouse_x, cy = s->mouse_y;

    for (int row = 0; row < 12; row++) {
        PRect r = { cx + offsets[row] - 1, cy + row - 1, widths[row] + 2, 1 };
        platform_fill_rect(r, black);
    }
    PRect bottom = { cx + offsets[11] - 1, cy + 12, widths[11] + 2, 1 };
    platform_fill_rect(bottom, black);

    for (int row = 0; row < 12; row++) {
        PRect r = { cx + offsets[row], cy + row, widths[row], 1 };
        platform_fill_rect(r, color_text());
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    ShellState s;
    memset(&s, 0, sizeof(s));
    s.sw = 1280;
    s.sh = 800;

    if (!platform_init(s.sw, s.sh, "Flolower OS v1.0")) return 1;
    s.sw = platform_width();
    s.sh = platform_height();
    s.mouse_x = s.sw / 2;
    s.mouse_y = s.sh / 2;

#ifdef HAVE_FS
    theme_load_settings();
#endif

    desktop_load_icons(&s);
    s.running = true;

    while (s.running) {
        PEvent e;
        while (platform_poll_event(&e)) {
            if (e.type == PEV_NONE) continue;
            if (e.type == PEV_MOUSE_MOVE || e.type == PEV_MOUSE_DOWN || e.type == PEV_MOUSE_UP) {
                s.mouse_x = e.x; s.mouse_y = e.y;
            }
            if (e.type == PEV_QUIT) { s.quit_dialog_open = true; continue; }
            if (e.type == PEV_RESIZE) { s.sw = e.x; s.sh = e.y; continue; }
            if (e.type == PEV_TEXT_INPUT) kb_layout_remap_text(e.text);
            if (e.type == PEV_KEY_DOWN && e.key == PK_ESCAPE) {
                if (s.quit_dialog_open) s.quit_dialog_open = false;
                else if (s.start_menu_open) startmenu_toggle(&s, 0);
                continue;
            }

            if (s.quit_dialog_open) {
                if (e.type == PEV_MOUSE_DOWN) handle_quit_dialog_click(&s, e.x, e.y);
                continue;
            }

            if (!s.start_menu_open && s.focused_window_id != 0 &&
                (e.type == PEV_TEXT_INPUT || e.type == PEV_KEY_DOWN || e.type == PEV_MOUSE_WHEEL)) {
                AppWindow *fw = wm_find_window(&s, s.focused_window_id);
                if (fw && !fw->minimized) {
                    if (e.type == PEV_TEXT_INPUT && fw->on_textinput) { fw->on_textinput(fw, e.text); continue; }
                    if (e.type == PEV_KEY_DOWN && fw->on_keydown) { fw->on_keydown(fw, e.key, e.mods); continue; }
                    if (e.type == PEV_MOUSE_WHEEL && fw->on_wheel) { fw->on_wheel(fw, e.wheel_dy); continue; }
                }
            }

            if (startmenu_handle_event(&s, &e)) continue;
            if (taskbar_handle_event(&s, &e)) continue;
            if (wm_handle_event(&s, &e)) continue;
            if (desktop_handle_event(&s, &e)) continue;
        }

        platform_clear(color_bg());

        desktop_render(&s);
        desktop_render_overlay(&s);
        wm_render_windows(&s);
        taskbar_render(&s);
        startmenu_render(&s);
        notify_render(&s);
        render_quit_dialog(&s);
        render_cursor(&s);

        platform_present();
        platform_delay_ms(16);
    }

    platform_shutdown();
    return 0;
}
