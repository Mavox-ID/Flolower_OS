#include "platform.h"
#include "font8x8_basic.h"
#include "font8x8_cyrillic.h"
#include "utf8.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define FONT_SCALE 2
#define GLYPH_W (8 * FONT_SCALE)
#define GLYPH_H (8 * FONT_SCALE)

static SDL_Window   *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static int g_w = 0, g_h = 0;
static int g_volume = 80;

void platform_set_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_volume = percent;
}
int platform_get_volume(void) { return g_volume; }

#ifdef __linux__
#include <dirent.h>
#include <string.h>

bool platform_battery_status(int *out_percent, bool *out_charging) {
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return false;

    struct dirent *ent;
    bool found = false;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] != 'B') continue;
        if (strncmp(ent->d_name, "BAT", 3) != 0) continue;

        char path[320];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", ent->d_name);
        FILE *fcap = fopen(path, "r");
        if (!fcap) continue;
        int cap = -1;
        if (fscanf(fcap, "%d", &cap) != 1) cap = -1;
        fclose(fcap);
        if (cap < 0) continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", ent->d_name);
        FILE *fstat = fopen(path, "r");
        bool charging = false;
        if (fstat) {
            char status[32] = {0};
            if (fgets(status, sizeof(status), fstat)) {
                charging = (strncmp(status, "Charging", 8) == 0);
            }
            fclose(fstat);
        }

        if (out_percent) *out_percent = cap;
        if (out_charging) *out_charging = charging;
        found = true;
        break;
    }
    closedir(d);
    return found;
}
#else
bool platform_battery_status(int *out_percent, bool *out_charging) {
    (void)out_percent; (void)out_charging;
    return false;
}
#endif

bool platform_init(int width, int height, const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    g_w = width; g_h = height;
    g_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) return false;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    SDL_StartTextInput();
    SDL_ShowCursor(SDL_DISABLE);
    return g_renderer != NULL;
}

void platform_shutdown(void) {
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}

int platform_width(void)  { return g_w; }
int platform_height(void) { return g_h; }

uint32_t platform_ticks_ms(void) { return SDL_GetTicks(); }
void     platform_delay_ms(int ms) { SDL_Delay((Uint32)ms); }

void platform_clear(PColor c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, 255);
    SDL_RenderClear(g_renderer);
}

void platform_fill_rect(PRect r, PColor c) {
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(g_renderer, &sr);
}

void platform_draw_rect_outline(PRect r, PColor c) {
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, 255);
    SDL_RenderDrawRect(g_renderer, &sr);
}

void platform_present(void) {
    SDL_RenderPresent(g_renderer);
}

int platform_text_width(const char *s) {
    if (!s) return 0;
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        p += utf8_decode(p, &cp);
        n++;
    }
    return n * GLYPH_W;
}
int platform_text_height(void) { return GLYPH_H; }

static void draw_glyph_rows(const unsigned char *rows, int x, int y, PColor c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, 255);
    for (int row = 0; row < 8; row++) {
        unsigned char bits = rows[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                SDL_Rect px = { x + col * FONT_SCALE, y + row * FONT_SCALE, FONT_SCALE, FONT_SCALE };
                SDL_RenderFillRect(g_renderer, &px);
            }
        }
    }
}

static void draw_glyph_cp(unsigned int cp, int x, int y, PColor c) {
    const unsigned char *rows;
    if (cp < 128) {
        rows = (const unsigned char *)font8x8_basic[cp];
    } else {
        rows = cyrillic_glyph_rows(cp);
        if (!rows) rows = (const unsigned char *)font8x8_basic[(int)'?'];
    }
    draw_glyph_rows(rows, x, y, c);
}

void platform_draw_text(const char *s, int x, int y, PColor c) {
    if (!s) return;
    int cx = x;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        p += utf8_decode(p, &cp);
        draw_glyph_cp(cp, cx, y, c);
        cx += GLYPH_W;
    }
}

static PKeycode map_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_LEFT: return PK_LEFT;
        case SDLK_RIGHT: return PK_RIGHT;
        case SDLK_UP: return PK_UP;
        case SDLK_DOWN: return PK_DOWN;
        case SDLK_RETURN: case SDLK_KP_ENTER: return PK_RETURN;
        case SDLK_BACKSPACE: return PK_BACKSPACE;
        case SDLK_DELETE: return PK_DELETE;
        case SDLK_ESCAPE: return PK_ESCAPE;
        case SDLK_TAB: return PK_TAB;
        case SDLK_s: return PK_S;
        default: return PK_OTHER;
    }
}

void platform_beep(int freq_hz, int duration_ms) {
    if (freq_hz <= 0 || duration_ms <= 0) return;
    if (g_volume <= 0) return;

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 44100;
    spec.format = AUDIO_S16SYS;
    spec.channels = 1;
    spec.samples = 2048;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!dev) return;

    int n_samples = spec.freq * duration_ms / 1000;
    int16_t *buf = (int16_t *)malloc((size_t)n_samples * sizeof(int16_t));
    if (!buf) { SDL_CloseAudioDevice(dev); return; }

    int period_samples = spec.freq / freq_hz;
    if (period_samples < 2) period_samples = 2;
    int16_t amp = (int16_t)(3000 * g_volume / 100);
    for (int i = 0; i < n_samples; i++) {
        int phase = i % period_samples;
        buf[i] = (phase < period_samples / 2) ? amp : (int16_t)-amp;
    }

    SDL_QueueAudio(dev, buf, (Uint32)(n_samples * (int)sizeof(int16_t)));
    SDL_PauseAudioDevice(dev, 0);
    free(buf);
}

bool platform_poll_event(PEvent *out) {
    SDL_Event e;
    if (!SDL_PollEvent(&e)) return false;
    out->x = out->y = out->button = out->wheel_dy = out->mods = 0;
    out->key = PK_NONE;
    out->text[0] = '\0';

    switch (e.type) {
        case SDL_QUIT:
            out->type = PEV_QUIT;
            return true;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GetWindowSize(g_window, &g_w, &g_h);
                out->type = PEV_RESIZE;
                out->x = g_w; out->y = g_h;
                return true;
            }
            out->type = PEV_NONE;
            return true;
        case SDL_MOUSEMOTION:
            out->type = PEV_MOUSE_MOVE;
            out->x = e.motion.x; out->y = e.motion.y;
            return true;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            out->type = (e.type == SDL_MOUSEBUTTONDOWN) ? PEV_MOUSE_DOWN : PEV_MOUSE_UP;
            out->x = e.button.x; out->y = e.button.y;
            out->button = (e.button.button == SDL_BUTTON_LEFT) ? PBTN_LEFT
                        : (e.button.button == SDL_BUTTON_RIGHT) ? PBTN_RIGHT : 0;
            return true;
        case SDL_MOUSEWHEEL:
            out->type = PEV_MOUSE_WHEEL;
            out->wheel_dy = e.wheel.y;
            return true;
        case SDL_KEYDOWN: {
            out->type = PEV_KEY_DOWN;
            out->key = map_key(e.key.keysym.sym);
            SDL_Keymod m = SDL_GetModState();
            if (m & (KMOD_CTRL | KMOD_GUI)) out->mods |= PMOD_CTRL;
            return true;
        }
        case SDL_TEXTINPUT:
            out->type = PEV_TEXT_INPUT;
            for (int i = 0; i < 7 && e.text.text[i]; i++) out->text[i] = e.text.text[i];
            return true;
        default:
            out->type = PEV_NONE;
            return true;
    }
}
