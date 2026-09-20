#include "shell.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>

#define SMILEY_SIZE 44

typedef struct {
    int smiley_x, smiley_y;
    int score;
    uint32_t rng_state;
} GameState;

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static uint32_t next_rand(GameState *g) {
    g->rng_state = g->rng_state * 1103515245u + 12345u;
    return (g->rng_state >> 16) & 0x7fff;
}

static PRect play_area_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 6, w->rect.y + TITLEBAR_H + 36, w->rect.w - 12,
                     w->rect.h - TITLEBAR_H - 44 };
}

static void spawn_smiley(GameState *g, PRect area) {
    int max_x = area.w - SMILEY_SIZE;
    int max_y = area.h - SMILEY_SIZE;
    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;
    g->smiley_x = (int)(next_rand(g) % (uint32_t)max_x);
    g->smiley_y = (int)(next_rand(g) % (uint32_t)max_y);
}

static void game_render(AppWindow *w) {
    GameState *g = (GameState *)w->user_data;

    char score_line[64];
    snprintf(score_line, sizeof(score_line), "Score: %d", g->score);
    PRect score_box = { w->rect.x + 6, w->rect.y + TITLEBAR_H + 6, w->rect.w - 12, 24 };
    draw_text(score_line, score_box.x, score_box.y, color_text());

    PRect area = play_area_rect(w);
    platform_fill_rect(area, (PColor){ 0x0c, 0x0a, 0x12 });

    PRect smiley = { area.x + g->smiley_x, area.y + g->smiley_y, SMILEY_SIZE, SMILEY_SIZE };
    platform_fill_rect(smiley, (PColor){ 0xff, 0xd4, 0x4d });
    draw_text_centered(":)", smiley, (PColor){ 0x0f, 0x0b, 0x18 });
}

static void game_click(AppWindow *w, int lx, int ly) {
    GameState *g = (GameState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    PRect area = play_area_rect(w);
    PRect smiley = { area.x + g->smiley_x, area.y + g->smiley_y, SMILEY_SIZE, SMILEY_SIZE };
    if (point_in(smiley, mx, my)) {
        g->score++;
        spawn_smiley(g, area);
    }
}

void app_open_smile_game(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Smile Game", 0.35f, 0.4f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    GameState *g = calloc(1, sizeof(GameState));
    g->rng_state = platform_ticks_ms() | 1;
    PRect area = play_area_rect(w);
    spawn_smiley(g, area);

    w->user_data = g;
    w->on_render = game_render;
    w->on_click = game_click;
}
