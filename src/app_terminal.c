/* app_terminal.c - real shell terminal (popen) */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "osk.h"
#include "desktop.h"
#include "common.h"

#define TERM_ROWS 28
#define TERM_COLS 96
#define OUT_LINES 400

typedef struct {
    char lines[OUT_LINES][TERM_COLS];
    int  head;                /* next write index (ring) */
    int  count;
    char input[256];
} Term;

static Term S_t;
static void term_draw(Win *w);   /* forward decl */

static void term_push_line(const char *s)
{
    char tmp[TERM_COLS];
    /* wrap long lines */
    while (strlen(s) > TERM_COLS - 1) {
        int take = TERM_COLS - 1;
        /* avoid splitting mid-word roughly: back up to space */
        for (int i = take; i > take / 2; i--) if (s[i] == ' ') { take = i; break; }
        memcpy(tmp, s, (size_t)take); tmp[take] = 0;
        psa_strcpy(S_t.lines[S_t.head], sizeof(S_t.lines[S_t.head]), tmp);
        S_t.head = (S_t.head + 1) % OUT_LINES;
        if (S_t.count < OUT_LINES) S_t.count++;
        s += take;
        while (*s == ' ') s++;
    }
    psa_strcpy(S_t.lines[S_t.head], sizeof(S_t.lines[S_t.head]), s);
    S_t.head = (S_t.head + 1) % OUT_LINES;
    if (S_t.count < OUT_LINES) S_t.count++;
}

static void banner(void)
{
    term_push_line("PS5GreenApple shell - sh via popen()");
    term_push_line("Type 'help' for commands. L1: close keyboard, R1: open keyboard.");
    term_push_line("");
}

void app_terminal_run(const char *cmd)
{
    if (!cmd || !*cmd) return;
    char prompt[300];
    psa_snprintf(prompt, sizeof(prompt), "PS5:/> %s", cmd);
    term_push_line(prompt);

#ifdef NO_SHELL
    term_push_line("shell disabled (built with NO_SHELL)");
#else
    char cmdline[300];
    psa_snprintf(cmdline, sizeof(cmdline), "%s 2>&1", cmd);
    FILE *f = popen(cmdline, "r");
    if (!f) { term_push_line("popen failed"); return; }
    char buf[512];
    int n = 0;
    while (fgets(buf, sizeof(buf), f) && n < 64) {
        size_t l = strlen(buf);
        while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
        if (l) { term_push_line(buf); n++; }
    }
    pclose(f);
    if (!n) term_push_line("(no output)");
#endif
    term_push_line("");
}

void app_terminal_open(void)
{
    if (!wm_find(WIN_TERMINAL)) {
        memset(&S_t, 0, sizeof(S_t));
        banner();
        app_terminal_run("uname -a");
        wm_open(WIN_TERMINAL, "Terminal", CHROME_MAC, 360, 130, 1200, 720, term_draw);
    } else {
        wm_raise(wm_find(WIN_TERMINAL));
    }
}

static void term_quick(int id, const char *cmd, int x, int y, int w, int h)
{
    if (ui_button(id, x, y, w, h, cmd, 0)) { /* visual only */ }
    if (ui_pressed(id)) app_terminal_run(cmd);
}

void term_draw(Win *w)
{
    int pad = 20;
    int x = w->x + pad, y = w->y + 58;
    int cw = w->w - 2 * pad;
    int ch = w->h - 58 - 150;

    /* output area */
    gfx_round_rect(x, y, cw, ch, 10, RGB(12, 12, 16));
    gfx_push_clip(x + 1, y + 1, cw - 2, ch - 2);
    int line_h = 21;
    int max_lines = (ch - 16) / line_h;
    int start = S_t.count - max_lines;
    if (start < 0) start = 0;
    for (int i = 0; i < max_lines && start + i < S_t.count; i++) {
        const char *ln = S_t.lines[(S_t.head - S_t.count + start + i + 2 * OUT_LINES) % OUT_LINES];
        uint32_t c = !strncmp(ln, "PS5:/> ", 7) ? RGB(120, 220, 130) : RGB(200, 205, 215);
        font_draw(FONT_MONO, ln, x + 12, y + 10 + i * line_h, 16, c);
    }
    gfx_pop_clip();

    /* quick commands */
    int qy = y + ch + 12;
    term_quick(300, "help",    x,          qy, 110, 38);
    term_quick(301, "ls",      x + 118,    qy, 110, 38);
    term_quick(302, "id",      x + 236,    qy, 110, 38);
    term_quick(303, "sysinfo", x + 354,    qy, 130, 38);

    /* input line */
    int iy = qy + 48;
    gfx_round_rect(x, iy, cw, 44, 10, g_theme.input_bg);
    font_draw(FONT_MONO, "PS5:/>", x + 12, iy + 12, 17, RGB(120, 220, 130));
    font_draw(FONT_MONO, S_t.input, x + 86, iy + 12, 17, RGB(230, 230, 240));
    if (!osk_active()) {
        const char *hint = "X: keyboard";
        font_draw(FONT_UI, hint, x + cw - font_text_w(FONT_UI, hint, 15) - 14, iy + 14, 15, g_theme.text_dim);
    }
    ui_add_hotspot(304, x, iy, cw, 44, "terminal-input");

    if (ui_pressed(304) || (in_clicked(BTN_R1) && !osk_active())) {
        osk_open(S_t.input, sizeof(S_t.input), "Terminal - type a command", OSK_FLAG_ENTER_CLOSES);
    }
    /* command submitted? (keyboard may already be closed by Enter) */
    if (osk_enter_pressed()) {
        char cmd[256];
        psa_strcpy(cmd, sizeof(cmd), S_t.input);
        S_t.input[0] = 0;
        app_terminal_run(cmd);
    }
}
