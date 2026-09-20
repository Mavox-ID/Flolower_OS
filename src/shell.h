#ifndef SHELL_H
#define SHELL_H

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_WINDOWS   32
#define MAX_ICONS     32
#define MAX_NOTIFS    8
#define MAX_TITLE     128

typedef struct AppWindow {
    bool     used;
    char     title[MAX_TITLE];
    PRect    rect;
    PRect    normal_rect;
    bool     is_max;
    bool     minimized;
    bool     dragging;
    bool     content_dragging;
    int      drag_off_x, drag_off_y;
    uint32_t id;

    void (*on_render)(struct AppWindow *w);
    void (*on_click)(struct AppWindow *w, int local_x, int local_y);
    void (*on_drag)(struct AppWindow *w, int local_x, int local_y);
    void (*on_rightclick)(struct AppWindow *w, int local_x, int local_y);
    void (*on_textinput)(struct AppWindow *w, const char *text);
    void (*on_keydown)(struct AppWindow *w, PKeycode key, int mods);
    void (*on_wheel)(struct AppWindow *w, int dy);
    void  *user_data;
} AppWindow;

typedef struct DesktopIcon {
    bool used;
    char emoji_label[8];
    char title[64];
    char key[64];
    int  x, y;
    bool dragging;
    int  drag_off_x, drag_off_y;
} DesktopIcon;

typedef struct Notification {
    bool     used;
    char     message[160];
    uint32_t shown_at;
    uint32_t duration_ms;
} Notification;

typedef struct ShellState {
    int  sw, sh;
    bool running;

    int  mouse_x, mouse_y;

    AppWindow windows[MAX_WINDOWS];
    uint32_t  next_window_id;
    uint32_t  focused_window_id;

    DesktopIcon icons[MAX_ICONS];
    Notification notifs[MAX_NOTIFS];

    bool  start_menu_open;
    float start_menu_anim;

    bool  quit_dialog_open;

    uint32_t clock_last_update;
    char     clock_text[16];

    bool     virtual_time_set;
    uint32_t virtual_base_sec;
    uint32_t virtual_base_ticks_ms;
} ShellState;

AppWindow *wm_create_window(ShellState *s, const char *title, float w_ratio, float h_ratio);
void       wm_close_window(ShellState *s, AppWindow *w);
void       wm_render_windows(ShellState *s);
bool       wm_handle_event(ShellState *s, PEvent *e);
AppWindow *wm_find_window(ShellState *s, uint32_t id);

void desktop_load_icons(ShellState *s);
void desktop_reset_layout(ShellState *s);
void desktop_save_icons(ShellState *s);
void desktop_add_icon(ShellState *s, const char *label, const char *title, const char *key, int x, int y);
void desktop_render(ShellState *s);
void desktop_render_overlay(ShellState *s);
bool desktop_handle_event(ShellState *s, PEvent *e);

void taskbar_render(ShellState *s);
bool taskbar_handle_event(ShellState *s, PEvent *e);

void startmenu_render(ShellState *s);
bool startmenu_handle_event(ShellState *s, PEvent *e);
void startmenu_toggle(ShellState *s, int force );

#define KB_LAYOUT_EN 0
#define KB_LAYOUT_UA 1
#define KB_LAYOUT_RU 2
void        kb_layout_set(int layout);
void        kb_layout_toggle(void);
int         kb_layout_current(void);
const char *kb_layout_name(void);
void        kb_layout_remap_text(char *text);

void notify_show(ShellState *s, const char *message, uint32_t duration_ms);
void notify_render(ShellState *s);

void draw_text(const char *text, int x, int y, PColor color);
void draw_text_centered(const char *text, PRect box, PColor color);
int  text_width(const char *text);

void app_launch(ShellState *s, const char *key, const char *display_title);

void app_open_calculator(ShellState *s);
void app_open_paint(ShellState *s);
void app_open_calendar(ShellState *s);
void app_open_taskmanager(ShellState *s);
void app_open_timer(ShellState *s);
void app_open_settings(ShellState *s);
void app_open_smile_game(ShellState *s);
#ifdef HAVE_FS
void app_open_terminal(ShellState *s);
void app_open_files(ShellState *s);
void app_open_text_editor(ShellState *s, const char *path);
void app_open_browser(ShellState *s);
void app_open_notes(ShellState *s);
void app_open_trash(ShellState *s);
#endif

#ifdef HAVE_FS
void persist_save_icon_positions(ShellState *s);
void persist_load_icon_positions(ShellState *s);
#endif

#endif
