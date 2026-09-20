#ifdef HAVE_FS
#include "shell.h"
#include <stdio.h>
#include <string.h>

#define ICON_POS_FILE "flolower_icons_pos.cfg"

void persist_save_icon_positions(ShellState *s) {
    FILE *f = fopen(ICON_POS_FILE, "w");
    if (!f) return;
    for (int i = 0; i < MAX_ICONS; i++) {
        DesktopIcon *ic = &s->icons[i];
        if (!ic->used) continue;
        fprintf(f, "%s=%d,%d\n", ic->key, ic->x, ic->y);
    }
    fclose(f);
}

void persist_load_icon_positions(ShellState *s) {
    FILE *f = fopen(ICON_POS_FILE, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64]; int x, y;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        size_t klen = (size_t)(eq - line);
        if (klen >= sizeof(key)) continue;
        memcpy(key, line, klen);
        key[klen] = '\0';
        if (sscanf(eq + 1, "%d,%d", &x, &y) != 2) continue;

        for (int i = 0; i < MAX_ICONS; i++) {
            if (s->icons[i].used && strcmp(s->icons[i].key, key) == 0) {
                s->icons[i].x = x;
                s->icons[i].y = y;
                break;
            }
        }
    }
    fclose(f);
}
#endif
