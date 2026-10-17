/* osk.c - QWERTY + symbols on-screen keyboard */
#include "osk.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"

static char  *S_buf;
static int    S_cap;
static char   S_title[64];
static int    S_flags;
static int    S_page;          /* 0 = letters, 1 = symbols */
static int    S_enter;
static int    S_defer_first_key;

#define OSK_ID_BASE 800

static const char *ROWS_LET[3] = { "1234567890", "qwertyuiop", "asdfghjkl" };
static const char *ROW_LET_BOT = "zxcvbnm";
static const char *ROWS_SYM[3] = { "!@#$%^&*+", "-_=[]{}/\\|", ":;\"'`<>,." };
static const char *ROW_SYM_BOT = ".?/~^";

#define KEY_W 62
#define KEY_H 58
#define KEY_GAP 8

static int key_count_row(int row)
{
    const char **rows = S_page == 0 ? ROWS_LET : ROWS_SYM;
    const char *bot  = S_page == 0 ? ROW_LET_BOT : ROW_SYM_BOT;
    if (row < 3) return (int)strlen(rows[row]);
    return (int)strlen(bot);
}

static int osk_key_id(int row, int col) { return OSK_ID_BASE + row * 16 + col; }

static void layout_rect(int row, int col, int *kx, int *ky, char *outch, int outn)
{
    const char **rows = S_page == 0 ? ROWS_LET : ROWS_SYM;
    const char *bot  = S_page == 0 ? ROW_LET_BOT : ROW_SYM_BOT;
    const char *r = row < 3 ? rows[row] : bot;
    int n = key_count_row(row);
    if (col >= n) { *outch = 0; return; }
    *outch = r[col];
    int panel_w = 10 * KEY_W + 9 * KEY_GAP;
    int row_w = n * KEY_W + (n - 1) * KEY_GAP;
    int x0 = (PSA_W - panel_w) / 2 + (panel_w - row_w) / 2;
    int y0 = PSA_H - 452;
    *kx = x0 + col * (KEY_W + KEY_GAP);
    *ky = y0 + row * (KEY_H + KEY_GAP);
}

void osk_open(char *buf, int cap, const char *title, int flags)
{
    S_buf = buf; S_cap = cap;
    psa_strcpy(S_title, sizeof(S_title), title);
    S_flags = flags; S_page = 0; S_enter = 0;
    S_defer_first_key = 1;   /* applied at end of osk_frame, once keys exist */
}

void osk_close(void) { S_buf = NULL; }
int  osk_active(void) { return S_buf != NULL; }
int  osk_enter_pressed(void) { int e = S_enter; S_enter = 0; return e; }

void osk_inject_char(char c)
{
    if (!S_buf) return;
    int len = (int)strlen(S_buf);
    if (len + 1 < S_cap) { S_buf[len] = c; S_buf[len + 1] = 0; }
}

void osk_inject_str(const char *s) { for (; s && *s; s++) osk_inject_char(*s); }

static void push_char(char c)
{
    if (!S_buf) return;
    int len = (int)strlen(S_buf);
    if (len + 1 < S_cap) { S_buf[len] = c; S_buf[len + 1] = 0; }
}

static void backspace(void)
{
    if (!S_buf) return;
    int len = (int)strlen(S_buf);
    if (len > 0) S_buf[len - 1] = 0;
}

void osk_frame(void)
{
    if (!S_buf) return;
    int panel_w = 10 * KEY_W + 9 * KEY_GAP;
    int x0 = (PSA_W - panel_w) / 2, y0 = PSA_H - 452;

    /* dim + panel */
    gfx_rect(0, 0, PSA_W, PSA_H, RGBA(0, 0, 0, 120));
    gfx_round_rect(x0 - 24, y0 - 74, panel_w + 48, 540, 18, RGBA(44, 44, 48, 245));

    int tw = font_text_w(FONT_BOLD, S_title, 22);
    font_draw(FONT_BOLD, S_title, x0 + (panel_w - tw) / 2, y0 - 60, 22, g_theme.text_bright);

    /* input preview */
    gfx_round_rect(x0, y0 - 34, panel_w, 30, 8, g_theme.input_bg);
    gfx_push_clip(x0 + 1, y0 - 33, panel_w - 2, 28);
    font_draw(FONT_MONO, S_buf, x0 + 10, y0 - 30, 18, RGB(120, 220, 130));
    gfx_pop_clip();

    /* key rows */
    for (int row = 0; row < 4; row++) {
        int n = key_count_row(row);
        for (int col = 0; col < n; col++) {
            int kx = 0, ky = 0; char ch[2] = {0, 0};
            layout_rect(row, col, &kx, &ky, ch, 2);
            int id = osk_key_id(row, col);
            int sel = (ui_sel_id() == id);
            gfx_round_rect(kx, ky, KEY_W, KEY_H, 8, sel ? g_theme.accent : RGB(58, 58, 64));
            int cw = font_text_w(FONT_UI, ch, 22);
            font_draw(FONT_UI, ch, kx + (KEY_W - cw) / 2, ky + (KEY_H - 26) / 2, 22, RGB(240, 240, 245));
            ui_add_hotspot(id, kx, ky, KEY_W, KEY_H, ch);
            if (ui_pressed(id)) push_char(ch[0]);
        }
    }

    /* bottom row: [sym] [space wide] [bksp] [enter] */
    int by = y0 + 4 * (KEY_H + KEY_GAP) + 4;
    int bx = x0;
    struct { const char *lbl; int w; int id; } extra[4] = {
        { S_page == 0 ? "#+=" : "ABC", 150, OSK_ID_BASE + 100 },
        { "space", panel_w - 150 - 130 - 130 - 3 * KEY_GAP, OSK_ID_BASE + 101 },
        { "bksp", 130, OSK_ID_BASE + 102 },
        { "enter", 130, OSK_ID_BASE + 103 },
    };
    for (int i = 0; i < 4; i++) {
        int sel = (ui_sel_id() == extra[i].id);
        gfx_round_rect(bx, by, extra[i].w, KEY_H, 8, sel ? g_theme.accent : RGB(50, 50, 56));
        int lw = font_text_w(FONT_UI, extra[i].lbl, 20);
        font_draw(FONT_UI, extra[i].lbl, bx + (extra[i].w - lw) / 2, by + (KEY_H - 24) / 2, 20, RGB(240, 240, 245));
        ui_add_hotspot(extra[i].id, bx, by, extra[i].w, KEY_H, extra[i].lbl);
        bx += extra[i].w + KEY_GAP;
        if (ui_pressed_btn(extra[i].id, BTN_CROSS)) {
            if (extra[i].id == OSK_ID_BASE + 100) S_page ^= 1;
            else if (extra[i].id == OSK_ID_BASE + 101) push_char(' ');
            else if (extra[i].id == OSK_ID_BASE + 102) backspace();
            else if (extra[i].id == OSK_ID_BASE + 103) {
                if (S_flags & OSK_FLAG_ENTER_CLOSES) { S_enter = 1; osk_close(); }
                else push_char('\n');
            }
        }
    }

    /* letter keys via ui_pressed above; also allow circle to close */
    if (in_clicked(BTN_CIRCLE)) osk_close();

    if (S_defer_first_key) {
        ui_set_sel(osk_key_id(1, 0));   /* 'q' - list now contains all keys */
        S_defer_first_key = 0;
    }
}
