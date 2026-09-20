#include "shell.h"

void draw_text(const char *text, int x, int y, PColor color) {
    if (!text || !text[0]) return;
    platform_draw_text(text, x, y, color);
}

int text_width(const char *text) {
    if (!text || !text[0]) return 0;
    return platform_text_width(text);
}

void draw_text_centered(const char *text, PRect box, PColor color) {
    if (!text || !text[0]) return;
    int w = platform_text_width(text);
    int h = platform_text_height();
    int x = box.x + (box.w - w) / 2;
    int y = box.y + (box.h - h) / 2;
    platform_draw_text(text, x, y, color);
}
