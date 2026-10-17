/* ui.c - focus navigation + buttons */
#include "ui.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include <math.h>

static Hotspot S_hs[UI_MAX_HOTSPOTS];
static int S_n = 0;
static int S_sel = 0;   /* hotspot id, 0 = none */

void ui_begin_frame(void) { S_n = 0; }

void ui_add_hotspot(int id, int x, int y, int w, int h, const char *label)
{
    if (S_n >= UI_MAX_HOTSPOTS) return;
    Hotspot *hsp = &S_hs[S_n++];
    hsp->id = id; hsp->x = x; hsp->y = y; hsp->w = w; hsp->h = h;
    psa_strcpy(hsp->label, sizeof(hsp->label), label ? label : "");
}

int ui_hotspot_count(void) { return S_n; }
Hotspot *ui_hotspot_at(int i) { return (i >= 0 && i < S_n) ? &S_hs[i] : NULL; }

int ui_sel_id(void) { return S_sel; }

void ui_set_sel(int id)
{
    if (id == 0) { S_sel = 0; return; }
    for (int i = 0; i < S_n; i++)
        if (S_hs[i].id == id) { S_sel = id; return; }
}

static Hotspot *find(int id)
{
    for (int i = 0; i < S_n; i++)
        if (S_hs[i].id == id) return &S_hs[i];
    return NULL;
}

void ui_nav(int dir)
{
    if (S_n == 0) { S_sel = 0; return; }
    Hotspot *cur = find(S_sel);
    if (!cur) { S_sel = S_hs[0].id; return; }
    float cx = cur->x + cur->w / 2.f, cy = cur->y + cur->h / 2.f;
    Hotspot *best = NULL;
    float best_score = 1e30f;
    for (int i = 0; i < S_n; i++) {
        Hotspot *o = &S_hs[i];
        if (o->id == S_sel) continue;
        float ox = o->x + o->w / 2.f - cx, oy = o->y + o->h / 2.f - cy;
        float primary, secondary;
        switch (dir) {
        case 0: primary = -oy; secondary = fabsf(ox); break;  /* up */
        case 1: primary =  oy; secondary = fabsf(ox); break;  /* down */
        case 2: primary = -ox; secondary = fabsf(oy); break;  /* left */
        default:primary =  ox; secondary = fabsf(oy); break;  /* right */
        }
        if (primary < 8.f) continue;                    /* must move in direction */
        float score = primary + secondary * 2.0f;
        if (score < best_score) { best_score = score; best = o; }
    }
    if (best) S_sel = best->id;
}

int ui_pressed(int id)     { return S_sel == id && in_clicked(BTN_CROSS); }
int ui_pressed_btn(int id, int btn) { return S_sel == id && in_clicked(btn); }

void ui_sel_rect(int *x, int *y, int *w, int *h)
{
    Hotspot *h2 = find(S_sel);
    if (!h2) { *x = *y = *w = *h = 0; return; }
    *x = h2->x - 5; *y = h2->y - 5; *w = h2->w + 10; *h = h2->h + 10;
}

int ui_button(int id, int x, int y, int w, int h, const char *label, int primary)
{
    int sel = (S_sel == id);
    uint32_t bg = primary ? g_theme.accent : RGB(58, 58, 64);
    if (sel) bg = primary ? lerp_color(g_theme.accent, RGB(255, 255, 255), 0.25f) : RGB(76, 76, 84);
    gfx_round_rect(x, y, w, h, 10, bg);
    int tw = font_text_w(FONT_UI, label, 20);
    font_draw(FONT_UI, label, x + (w - tw) / 2, y + (h - 24) / 2, 20, RGB(255, 255, 255));
    if (sel) gfx_round_rect_outline(x - 5, y - 5, w + 10, h + 10, 13, 3, RGB(10, 132, 255));
    ui_add_hotspot(id, x, y, w, h, label);
    return sel;
}
