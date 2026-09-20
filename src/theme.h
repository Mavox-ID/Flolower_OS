#ifndef THEME_H
#define THEME_H

#include "platform.h"

extern PColor g_color_bg;
extern PColor g_color_accent;

static inline PColor color_bg(void)          { return g_color_bg; }
static inline PColor color_menu_bg(void)      { PColor c = {0x23,0x1a,0x30}; return c; }
static inline PColor color_menu_item(void)    { PColor c = {0x2b,0x20,0x36}; return c; }
static inline PColor color_taskbar_bg(void)   { PColor c = {0x18,0x12,0x1f}; return c; }
static inline PColor color_window_bg(void)    { PColor c = {0x24,0x1b,0x2b}; return c; }
static inline PColor color_title_bg(void)     { PColor c = {0x2a,0x20,0x30}; return c; }
static inline PColor color_text(void)         { PColor c = {0xff,0xff,0xff}; return c; }
static inline PColor color_text_dim(void)     { PColor c = {0x88,0x88,0x88}; return c; }
static inline PColor color_accent(void)       { return g_color_accent; }
static inline PColor color_danger(void)       { PColor c = {0xff,0x44,0x44}; return c; }
static inline PColor color_task_active(void)  { PColor c = {0x3a,0x2d,0x4a}; return c; }

#define TASKBAR_H   36
#define TITLEBAR_H  28
#define ICON_W      80
#define ICON_H      100
#define MENU_W      320
#define MENU_ITEM_H 32

void theme_reset_defaults(void);
void theme_set_bg(PColor c);
void theme_set_accent(PColor c);
#ifdef HAVE_FS
void theme_load_settings(void);
void theme_save_settings(void);
#endif

#endif
