#include "shell.h"
#include "theme.h"
#include <string.h>

void notify_show(ShellState *s, const char *message, uint32_t duration_ms) {
    for (int i = 0; i < MAX_NOTIFS; i++) {
        if (!s->notifs[i].used) {
            s->notifs[i].used = true;
            strncpy(s->notifs[i].message, message, sizeof(s->notifs[i].message) - 1);
            s->notifs[i].shown_at = platform_ticks_ms();
            s->notifs[i].duration_ms = duration_ms;
            return;
        }
    }
    s->notifs[0].used = true;
    strncpy(s->notifs[0].message, message, sizeof(s->notifs[0].message) - 1);
    s->notifs[0].shown_at = platform_ticks_ms();
    s->notifs[0].duration_ms = duration_ms;
}

void notify_render(ShellState *s) {
    uint32_t now = platform_ticks_ms();
    int stack_y = s->sh - TASKBAR_H - 16;
    for (int i = MAX_NOTIFS - 1; i >= 0; i--) {
        Notification *n = &s->notifs[i];
        if (!n->used) continue;
        uint32_t age = now - n->shown_at;
        if (age > n->duration_ms) { n->used = false; continue; }

        int w = 260, h = 40;
        PRect box = { s->sw - w - 16, stack_y - h, w, h };
        stack_y -= (h + 8);

        platform_fill_rect(box, color_menu_item());
        draw_text(n->message, box.x + 10, box.y + (h - platform_text_height()) / 2, color_text());
    }
}
