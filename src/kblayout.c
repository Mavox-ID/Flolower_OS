#include "shell.h"

typedef struct { char from; unsigned int to_cp; } KbMapEntry;

static const KbMapEntry UA_MAP[] = {
    {'q', 0x0419}, {'w', 0x0426}, {'e', 0x0423}, {'r', 0x041A}, {'t', 0x0415},
    {'y', 0x041D}, {'u', 0x0413}, {'i', 0x0428}, {'o', 0x0429}, {'p', 0x0417},
    {'[', 0x0425}, {']', 0x0407},
    {'a', 0x0424}, {'s', 0x0406}, {'d', 0x0412}, {'f', 0x0410}, {'g', 0x041F},
    {'h', 0x0420}, {'j', 0x041E}, {'k', 0x041B}, {'l', 0x0414}, {';', 0x0416}, {'\'', 0x0404},
    {'z', 0x042F}, {'x', 0x0427}, {'c', 0x0421}, {'v', 0x041C}, {'b', 0x0418},
    {'n', 0x0422}, {'m', 0x042C}, {',', 0x0411}, {'.', 0x042E},
};
#define N_UA_MAP (int)(sizeof(UA_MAP) / sizeof(UA_MAP[0]))

static const KbMapEntry RU_MAP[] = {
    {'q', 0x0419}, {'w', 0x0426}, {'e', 0x0423}, {'r', 0x041A}, {'t', 0x0415},
    {'y', 0x041D}, {'u', 0x0413}, {'i', 0x0428}, {'o', 0x0429}, {'p', 0x0417},
    {'[', 0x0425}, {']', 0x042A}, {'`', 0x0401},
    {'a', 0x0424}, {'s', 0x042B}, {'d', 0x0412}, {'f', 0x0410}, {'g', 0x041F},
    {'h', 0x0420}, {'j', 0x041E}, {'k', 0x041B}, {'l', 0x0414}, {';', 0x0416}, {'\'', 0x042D},
    {'z', 0x042F}, {'x', 0x0427}, {'c', 0x0421}, {'v', 0x041C}, {'b', 0x0418},
    {'n', 0x0422}, {'m', 0x042C}, {',', 0x0411}, {'.', 0x042E},
};
#define N_RU_MAP (int)(sizeof(RU_MAP) / sizeof(RU_MAP[0]))

static int g_kb_layout = KB_LAYOUT_EN;

void kb_layout_set(int layout) {
    if (layout == KB_LAYOUT_UA) g_kb_layout = KB_LAYOUT_UA;
    else if (layout == KB_LAYOUT_RU) g_kb_layout = KB_LAYOUT_RU;
    else g_kb_layout = KB_LAYOUT_EN;
}

void kb_layout_toggle(void) {
    if (g_kb_layout == KB_LAYOUT_EN) g_kb_layout = KB_LAYOUT_UA;
    else if (g_kb_layout == KB_LAYOUT_UA) g_kb_layout = KB_LAYOUT_RU;
    else g_kb_layout = KB_LAYOUT_EN;
}

int kb_layout_current(void) { return g_kb_layout; }

const char *kb_layout_name(void) {
    if (g_kb_layout == KB_LAYOUT_UA) return "UA";
    if (g_kb_layout == KB_LAYOUT_RU) return "RU";
    return "EN";
}

void kb_layout_remap_text(char *text) {
    if (g_kb_layout != KB_LAYOUT_UA && g_kb_layout != KB_LAYOUT_RU) return;
    if (!text[0] || text[1] != '\0') return;

    char lower = text[0];
    if (lower >= 'A' && lower <= 'Z') lower = (char)(lower - 'A' + 'a');

    const KbMapEntry *map = (g_kb_layout == KB_LAYOUT_UA) ? UA_MAP : RU_MAP;
    int n = (g_kb_layout == KB_LAYOUT_UA) ? N_UA_MAP : N_RU_MAP;

    for (int i = 0; i < n; i++) {
        if (map[i].from != lower) continue;
        unsigned int cp = map[i].to_cp;
        text[0] = (char)(0xC0 | (cp >> 6));
        text[1] = (char)(0x80 | (cp & 0x3F));
        text[2] = '\0';
        return;
    }
}
