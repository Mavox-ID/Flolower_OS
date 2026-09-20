#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>

#define QUICK_ROW_H   32
#define N_QUICK_ROWS  4
#define BATTERY_POLL_MS 2000

static uint32_t s_battery_last_poll = 0;
static int      s_battery_percent   = -1;
static bool     s_battery_charging  = false;
static bool     s_battery_available = false;

static void battery_poll(void) {
    uint32_t now = platform_ticks_ms();
    if (s_battery_last_poll != 0 && now - s_battery_last_poll < BATTERY_POLL_MS) return;
    s_battery_last_poll = now;
    s_battery_available = platform_battery_status(&s_battery_percent, &s_battery_charging);
}

typedef struct { const char *glyph; const char *title; const char *key; } MenuAppDef;
static const MenuAppDef MENU_APPS[] = {
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
#define N_MENU_APPS (int)(sizeof(MENU_APPS)/sizeof(MENU_APPS[0]))

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void startmenu_toggle(ShellState *s, int force) {
    if (force == -1) s->start_menu_open = !s->start_menu_open;
    else s->start_menu_open = (force == 1);
}

static PRect menu_rect(ShellState *s) {
    int h = N_MENU_APPS * MENU_ITEM_H + 40 + N_QUICK_ROWS * QUICK_ROW_H + 12;
    int shown_y = s->sh - TASKBAR_H - h;
    int hidden_y = s->sh;
    int y = (int)(hidden_y + (shown_y - hidden_y) * s->start_menu_anim);
    return (PRect){ 0, y, MENU_W, h };
}

void startmenu_render(ShellState *s) {
    float target = s->start_menu_open ? 1.0f : 0.0f;
    s->start_menu_anim += (target - s->start_menu_anim) * 0.35f;
    if (s->start_menu_anim < 0.01f && !s->start_menu_open) { s->start_menu_anim = 0.0f; return; }
    if (s->start_menu_anim > 0.995f) s->start_menu_anim = 1.0f;

    PRect box = menu_rect(s);
    platform_fill_rect(box, color_menu_bg());
    draw_text("Flolower OS", box.x + 12, box.y + 10, color_text());

    for (int i = 0; i < N_MENU_APPS; i++) {
        PRect row = { box.x, box.y + 40 + i * MENU_ITEM_H, MENU_W, MENU_ITEM_H };
        draw_text(MENU_APPS[i].glyph, row.x + 12, row.y + (MENU_ITEM_H - platform_text_height()) / 2, color_text());
        draw_text(MENU_APPS[i].title, row.x + 48, row.y + (MENU_ITEM_H - platform_text_height()) / 2, color_text());
    }

    battery_poll();
    int qy = box.y + 40 + N_MENU_APPS * MENU_ITEM_H + 8;
    int ty = (QUICK_ROW_H - platform_text_height()) / 2;

    PRect layout_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    platform_fill_rect(layout_row, color_menu_item());
    draw_text("LAYOUT", layout_row.x + 12, layout_row.y + ty, color_text());
    char layout_buf[8];
    snprintf(layout_buf, sizeof(layout_buf), "[%s]", kb_layout_name());
    draw_text(layout_buf, layout_row.x + MENU_W - 60, layout_row.y + ty, color_accent());
    qy += QUICK_ROW_H;

    PRect vol_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    platform_fill_rect(vol_row, color_menu_item());
    draw_text("VOLUME", vol_row.x + 12, vol_row.y + ty, color_text());
    int vol = platform_get_volume();
    char vol_buf[8];
    snprintf(vol_buf, sizeof(vol_buf), "%d%%", vol);
    PRect vol_minus = { vol_row.x + MENU_W - 108, vol_row.y + 4, 24, QUICK_ROW_H - 8 };
    PRect vol_plus  = { vol_row.x + MENU_W - 34,  vol_row.y + 4, 24, QUICK_ROW_H - 8 };
    platform_fill_rect(vol_minus, color_task_active());
    platform_fill_rect(vol_plus, color_task_active());
    draw_text_centered("-", vol_minus, color_text());
    draw_text_centered("+", vol_plus, color_text());
    draw_text(vol_buf, vol_row.x + MENU_W - 78, vol_row.y + ty, color_accent());
    qy += QUICK_ROW_H;

    PRect bat_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    platform_fill_rect(bat_row, color_menu_item());
    draw_text("BATTERY", bat_row.x + 12, bat_row.y + ty, color_text());
    char bat_buf[24];
    if (s_battery_available) {
        snprintf(bat_buf, sizeof(bat_buf), "%d%%%s", s_battery_percent, s_battery_charging ? " (charge)" : "");
    } else {
        snprintf(bat_buf, sizeof(bat_buf), "N/A");
    }
    draw_text(bat_buf, bat_row.x + MENU_W - platform_text_width(bat_buf) - 12,
               bat_row.y + ty, s_battery_available ? color_accent() : color_text_dim());
    qy += QUICK_ROW_H;

    PRect power_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    platform_fill_rect(power_row, color_menu_item());
    draw_text("SHUT DOWN", power_row.x + 12, power_row.y + ty, color_text());
}

bool startmenu_handle_event(ShellState *s, PEvent *e) {
    if (!s->start_menu_open && s->start_menu_anim <= 0.0f) return false;
    if (e->type != PEV_MOUSE_DOWN || e->button != PBTN_LEFT) return false;

    PRect box = menu_rect(s);
    if (!point_in(box, e->x, e->y)) {
        startmenu_toggle(s, 0);
        return true;
    }
    for (int i = 0; i < N_MENU_APPS; i++) {
        PRect row = { box.x, box.y + 40 + i * MENU_ITEM_H, MENU_W, MENU_ITEM_H };
        if (point_in(row, e->x, e->y)) {
            app_launch(s, MENU_APPS[i].key, MENU_APPS[i].title);
            startmenu_toggle(s, 0);
            return true;
        }
    }

    int qy = box.y + 40 + N_MENU_APPS * MENU_ITEM_H + 8;

    PRect layout_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    if (point_in(layout_row, e->x, e->y)) {
        kb_layout_toggle();
        return true;
    }
    qy += QUICK_ROW_H;

    PRect vol_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    PRect vol_minus = { vol_row.x + MENU_W - 108, vol_row.y + 4, 24, QUICK_ROW_H - 8 };
    PRect vol_plus  = { vol_row.x + MENU_W - 34,  vol_row.y + 4, 24, QUICK_ROW_H - 8 };
    if (point_in(vol_minus, e->x, e->y)) {
        platform_set_volume(platform_get_volume() - 10);
        return true;
    }
    if (point_in(vol_plus, e->x, e->y)) {
        platform_set_volume(platform_get_volume() + 10);
        platform_beep(880, 60);
        return true;
    }
    if (point_in(vol_row, e->x, e->y)) return true;
    qy += QUICK_ROW_H;
    qy += QUICK_ROW_H;

    PRect power_row = { box.x, qy, MENU_W, QUICK_ROW_H };
    if (point_in(power_row, e->x, e->y)) {
#ifdef PLATFORM_HOSTED
        notify_show(s, "Shutdown isn't available in the hosted build", 2500);
#else
        extern void system_shutdown(void);
        system_shutdown();
#endif
        return true;
    }

    return true;
}
