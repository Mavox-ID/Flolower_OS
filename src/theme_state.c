#include "theme.h"
#include "shell.h"
#ifdef HAVE_FS
#include <stdio.h>
#endif

#define DEFAULT_BG      ((PColor){0x0f,0x0b,0x18})
#define DEFAULT_ACCENT  ((PColor){0x4d,0x7f,0xff})

PColor g_color_bg     = {0x0f,0x0b,0x18};
PColor g_color_accent = {0x4d,0x7f,0xff};

void theme_reset_defaults(void) {
    g_color_bg = DEFAULT_BG;
    g_color_accent = DEFAULT_ACCENT;
}

void theme_set_bg(PColor c)     { g_color_bg = c; }
void theme_set_accent(PColor c) { g_color_accent = c; }

#ifdef HAVE_FS
#define SETTINGS_PATH "/settings.cfg"

void theme_save_settings(void) {
    FILE *f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "bg=%u,%u,%u\n", g_color_bg.r, g_color_bg.g, g_color_bg.b);
    fprintf(f, "accent=%u,%u,%u\n", g_color_accent.r, g_color_accent.g, g_color_accent.b);
    fprintf(f, "volume=%d\n", platform_get_volume());
    fprintf(f, "kb_layout=%s\n", kb_layout_name());
    fclose(f);
}

void theme_load_settings(void) {
    FILE *f = fopen(SETTINGS_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        unsigned r, g, b;
        if (sscanf(line, "bg=%u,%u,%u", &r, &g, &b) == 3) {
            g_color_bg = (PColor){ (uint8_t)r, (uint8_t)g, (uint8_t)b };
        } else if (sscanf(line, "accent=%u,%u,%u", &r, &g, &b) == 3) {
            g_color_accent = (PColor){ (uint8_t)r, (uint8_t)g, (uint8_t)b };
        } else {
            int vol;
            char layout[8];
            if (sscanf(line, "volume=%d", &vol) == 1) {
                platform_set_volume(vol);
            } else if (sscanf(line, "kb_layout=%7s", layout) == 1) {
                kb_layout_set(layout[0] == 'U' ? KB_LAYOUT_UA : KB_LAYOUT_EN);
            }
        }
    }
    fclose(f);
}
#endif
