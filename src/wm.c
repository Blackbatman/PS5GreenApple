/* wm.c - window compositing + three chrome styles */
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"

static Win S_wins[WM_MAX_WINDOWS];

void wm_init(void) { memset(S_wins, 0, sizeof(S_wins)); }

Win *wm_open(int id, const char *title, ChromeStyle ch, int x, int y, int w, int h,
             WinDraw draw)
{
    Win *w2 = wm_find(id);
    if (w2) { wm_raise(w2); return w2; }
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (S_wins[i].open) continue;
        memset(w2 = &S_wins[i], 0, sizeof(Win));
        w2->id = id; w2->open = 1;
        w2->x = x; w2->y = y; w2->w = w; w2->h = h;
        psa_strcpy(w2->title, sizeof(w2->title), title);
        w2->chrome = ch;
        w2->ws = 0;
        w2->draw = draw;
        w2->scroll_off = 0;
        /* clamp to screen */
        if (w2->x + w2->w > PSA_W - 24) w2->x = PSA_W - 24 - w2->w;
        if (w2->y + w2->h > PSA_H - 130) w2->y = PSA_H - 130 - h;
        if (w2->x < 24) w2->x = 24;
        if (w2->y < 64) w2->y = 64;
        return w2;
    }
    return NULL;
}

void wm_close(int id)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (S_wins[i].open && S_wins[i].id == id) { S_wins[i].open = 0; return; }
}

void wm_close_top(void)
{
    Win *t = wm_top();
    if (t) t->open = 0;
}

Win *wm_find(int id)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (S_wins[i].open && S_wins[i].id == id) return &S_wins[i];
    return NULL;
}

Win *wm_top(void)
{
    for (int i = WM_MAX_WINDOWS - 1; i >= 0; i--)
        if (S_wins[i].open) return &S_wins[i];
    return NULL;
}

void wm_raise(Win *w)
{
    if (!w || w < S_wins || w >= S_wins + WM_MAX_WINDOWS) return;
    /* move to end of array = highest z */
    int idx = (int)(w - S_wins);
    Win tmp = S_wins[idx];
    for (int i = idx; i < WM_MAX_WINDOWS - 1; i++) S_wins[i] = S_wins[i + 1];
    S_wins[WM_MAX_WINDOWS - 1] = tmp;
}

int wm_any_open(void)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (S_wins[i].open) return 1;
    return 0;
}

#define TB_H 52

static void draw_traffic_lights(Win *w, int focused)
{
    int x = w->x + 20, y = w->y + TB_H / 2;
    int r = 8;
    uint32_t cols[3] = { g_theme.red, g_theme.yellow, g_theme.green };
    for (int i = 0; i < 3; i++) {
        uint32_t c = focused ? cols[i] : RGB(90, 90, 96);
        gfx_circle_fill(x + i * 26, y, r, c);
        /* subtle inner shade */
        gfx_circle_fill(x + i * 26, y, r - 2, lerp_color(c, RGB(0, 0, 0), 0.18f));
        if (i == 0) {
            /* close button hotspot */
            ui_add_hotspot(60 + w->id, x - r, y - r, r * 2, r * 2, "close");
        }
    }
}

static void draw_titlebar(Win *w, int focused)
{
    int tb = TB_H;
    switch (w->chrome) {
    case CHROME_MAC:
        gfx_round_rect(w->x, w->y, w->w, tb, 12, focused ? g_theme.titlebar_bg : lerp_color(g_theme.titlebar_bg, RGB(20, 20, 22), 0.4f));
        gfx_rect(w->x, w->y + tb - 12, w->w, 12, focused ? g_theme.titlebar_bg : lerp_color(g_theme.titlebar_bg, RGB(20, 20, 22), 0.4f));
        gfx_round_rect(w->x, w->y + w->h - 24, w->w, 24, 12, g_theme.window_bg); /* re-round bottom corners area */
        gfx_rect(w->x + 1, w->y + tb - 2, w->w - 2, 1, g_theme.separator);
        draw_traffic_lights(w, focused);
        {
            int tw = font_text_w(FONT_BOLD, w->title, 20);
            font_draw(FONT_BOLD, w->title, w->x + (w->w - tw) / 2, w->y + (tb - 24) / 2, 20,
                      focused ? g_theme.titlebar_text : g_theme.text_dim);
        }
        break;
    case CHROME_WIN: {
        gfx_rect(w->x, w->y, w->w, tb, RGB(32, 32, 36));
        gfx_rect(w->x, w->y, w->w, 2, RGB(80, 120, 200));
        font_draw(FONT_UI, w->title, w->x + 18, w->y + (tb - 26) / 2, 20,
                  focused ? RGB(230, 230, 240) : g_theme.text_dim);
        /* caption buttons right: minimize, maximize, close */
        int bx = w->x + w->w - 46 * 3;
        for (int i = 0; i < 3; i++) {
            uint32_t bg = (i == 2) ? (focused ? RGB(196, 43, 28) : RGB(120, 40, 34)) : RGB(45, 45, 50);
            if (i == 2) bg = (focused ? RGB(196, 43, 28) : RGB(120, 40, 34));
            gfx_rect(bx + i * 46, w->y, 46, tb, bg);
            int cx2 = bx + i * 46 + 23, cy2 = w->y + tb / 2;
            if (i == 0) gfx_hline(cx2 - 7, cx2 + 7, cy2, RGB(230, 230, 240));
            else if (i == 1) gfx_rect_outline(cx2 - 7, cy2 - 7, 14, 14, 2, RGB(230, 230, 240));
            else {
                gfx_line(cx2 - 7, cy2 - 7, cx2 + 7, cy2 + 7, RGB(255, 255, 255));
                gfx_line(cx2 + 7, cy2 - 7, cx2 - 7, cy2 + 7, RGB(255, 255, 255));
            }
        }
        ui_add_hotspot(60 + w->id, bx + 2 * 46, w->y, 46, tb, "close");
        break;
    }
    case CHROME_LINUX: {
        gfx_rect(w->x, w->y, w->w, tb, RGB(34, 36, 40));
        gfx_rect(w->x, w->y, w->w, 1, RGB(70, 72, 80));
        font_draw(FONT_UI, w->title, w->x + 18, w->y + (tb - 26) / 2, 20,
                  focused ? RGB(235, 235, 245) : g_theme.text_dim);
        /* gnome headerbar close circle right */
        int bx = w->x + w->w - 46;
        gfx_circle_fill(bx + 23, w->y + tb / 2, 13, RGB(60, 60, 66));
        int cx2 = bx + 23, cy2 = w->y + tb / 2;
        gfx_line(cx2 - 5, cy2 - 5, cx2 + 5, cy2 + 5, RGB(240, 240, 245));
        gfx_line(cx2 + 5, cy2 - 5, cx2 - 5, cy2 + 5, RGB(240, 240, 245));
        ui_add_hotspot(60 + w->id, bx, w->y + tb / 2 - 18, 46, 36, "close");
        break;
    }
    }
}

void wm_draw_all(void)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        Win *w = &S_wins[i];
        if (!w->open) continue;
        int focused = (wm_top() == w);

        /* shadow */
        gfx_round_rect(w->x - 8, w->y - 4, w->w + 16, w->h + 20, 18, RGBA(0, 0, 0, 90));
        gfx_round_rect(w->x - 4, w->y - 2, w->w + 8, w->h + 12, 15, RGBA(0, 0, 0, 70));

        /* body */
        gfx_round_rect(w->x, w->y, w->w, w->h, 12, g_theme.window_bg);
        draw_titlebar(w, focused);

        /* content, clipped below titlebar */
        int ctop = (w->chrome == CHROME_MAC) ? TB_H - 10 : TB_H;
        gfx_push_clip(w->x + 1, w->y + ctop, w->w - 2, w->h - ctop - 1);
        if (w->draw) w->draw(w);
        gfx_pop_clip();

        gfx_round_rect_outline(w->x, w->y, w->w, w->h, 12, 1,
                               focused ? lerp_color(g_theme.window_border, g_theme.accent, 0.35f)
                                       : g_theme.window_border);
    }
}
