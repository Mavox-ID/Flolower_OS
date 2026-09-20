#include "shell.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CALC_MAX_EXPR 64

typedef struct {
    char expr[CALC_MAX_EXPR];
} CalcState;

typedef struct { const char *p; bool error; } Parser;

static void skip_ws(Parser *ps) { while (*ps->p == ' ') ps->p++; }
static double parse_expr(Parser *ps);

static double parse_factor(Parser *ps) {
    skip_ws(ps);
    if (*ps->p == '(') {
        ps->p++;
        double v = parse_expr(ps);
        skip_ws(ps);
        if (*ps->p == ')') ps->p++; else ps->error = true;
        return v;
    }
    if (*ps->p == '-') { ps->p++; return -parse_factor(ps); }
    if (*ps->p == '+') { ps->p++; return parse_factor(ps); }
    char *end;
    double v = strtod(ps->p, &end);
    if (end == ps->p) { ps->error = true; return 0; }
    ps->p = end;
    return v;
}

static double parse_term(Parser *ps) {
    double v = parse_factor(ps);
    for (;;) {
        skip_ws(ps);
        if (*ps->p == '*') { ps->p++; v *= parse_factor(ps); }
        else if (*ps->p == '/') {
            ps->p++;
            double d = parse_factor(ps);
            if (d == 0) { ps->error = true; return 0; }
            v /= d;
        } else break;
    }
    return v;
}

static double parse_expr(Parser *ps) {
    double v = parse_term(ps);
    for (;;) {
        skip_ws(ps);
        if (*ps->p == '+') { ps->p++; v += parse_term(ps); }
        else if (*ps->p == '-') { ps->p++; v -= parse_term(ps); }
        else break;
    }
    return v;
}

static bool calc_eval(const char *expr, double *out) {
    Parser ps = { expr, false };
    if (!expr[0]) return false;
    double v = parse_expr(&ps);
    skip_ws(&ps);
    if (ps.error || *ps.p != '\0') return false;
    *out = v;
    return true;
}

static void calc_format(double v, char *out, size_t n) {
    if (v == (long long)v && v > -1e15 && v < 1e15)
        snprintf(out, n, "%lld", (long long)v);
    else
        snprintf(out, n, "%.10g", v);
}

typedef struct { const char *label; int row, col; } CalcBtn;
static const CalcBtn CALC_BTNS[] = {
    {"C",0,0},{"(",0,1},{")",0,2},{"<-",0,3},
    {"7",1,0},{"8",1,1},{"9",1,2},{"/",1,3},
    {"4",2,0},{"5",2,1},{"6",2,2},{"*",2,3},
    {"1",3,0},{"2",3,1},{"3",3,2},{"-",3,3},
    {"0",4,0},{".",4,1},{"=",4,2},{"+",4,3},
};
#define N_CALC_BTNS (int)(sizeof(CALC_BTNS)/sizeof(CALC_BTNS[0]))

static PRect calc_display_rect(AppWindow *w) {
    return (PRect){ w->rect.x + 10, w->rect.y + TITLEBAR_H + 10, w->rect.w - 20, 40 };
}

static PRect calc_btn_rect(AppWindow *w, int row, int col) {
    int top = w->rect.y + TITLEBAR_H + 60;
    int pad = 6;
    int bw = (w->rect.w - 20 - pad * 3) / 4;
    int bh = 40;
    return (PRect){
        w->rect.x + 10 + col * (bw + pad),
        top + row * (bh + pad),
        bw, bh
    };
}

static bool point_in(PRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void calc_render(AppWindow *w) {
    CalcState *cs = (CalcState *)w->user_data;

    PRect disp = calc_display_rect(w);
    platform_fill_rect(disp, color_menu_item());
    const char *shown = cs->expr[0] ? cs->expr : "0";
    draw_text(shown, disp.x + 8, disp.y + (disp.h - platform_text_height()) / 2, color_text());

    for (int i = 0; i < N_CALC_BTNS; i++) {
        PRect b = calc_btn_rect(w, CALC_BTNS[i].row, CALC_BTNS[i].col);
        bool is_op = strchr("+-*/=", CALC_BTNS[i].label[0]) && CALC_BTNS[i].label[1] == '\0';
        platform_fill_rect(b, is_op ? color_task_active() : color_title_bg());
        draw_text_centered(CALC_BTNS[i].label, b, color_text());
    }
}

static void calc_click(AppWindow *w, int lx, int ly) {
    CalcState *cs = (CalcState *)w->user_data;
    int mx = w->rect.x + lx, my = w->rect.y + ly;

    for (int i = 0; i < N_CALC_BTNS; i++) {
        PRect b = calc_btn_rect(w, CALC_BTNS[i].row, CALC_BTNS[i].col);
        if (!point_in(b, mx, my)) continue;
        const char *label = CALC_BTNS[i].label;

        if (strcmp(label, "C") == 0) {
            cs->expr[0] = '\0';
        } else if (strcmp(label, "<-") == 0) {
            size_t len = strlen(cs->expr);
            if (len > 0) cs->expr[len - 1] = '\0';
        } else if (strcmp(label, "=") == 0) {
            double v;
            if (calc_eval(cs->expr, &v)) calc_format(v, cs->expr, sizeof(cs->expr));
            else snprintf(cs->expr, sizeof(cs->expr), "Error");
        } else {
            size_t len = strlen(cs->expr);
            if (len < sizeof(cs->expr) - 1) strcat(cs->expr, label);
        }
        return;
    }
}

void app_open_calculator(ShellState *s) {
    AppWindow *w = wm_create_window(s, "Calculator", 0.28f, 0.45f);
    if (!w) { notify_show(s, "Too many windows open", 2000); return; }

    static CalcState pool[MAX_WINDOWS];
    int idx = (int)(w - s->windows);
    CalcState *cs = &pool[idx];
    memset(cs, 0, sizeof(*cs));

    w->user_data = cs;
    w->on_render = calc_render;
    w->on_click = calc_click;
}
