/* app_nexus.c – NEXUS OS Selection Hub
 * Mass Effect / Star Trek inspired command interface
 * Realistic-styled Apple, opening Windows, and layered ice-cream Linux icons
 */

#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "ui.h"
#include "desktop.h"
#include "common.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- Realistic Icon Drawing ---------- */

static void draw_nexus_apple_realistic(int cx, int cy, int size, float t)
{
    int r = size / 2;
    float pulse = 0.85f + 0.15f * sinf(t * 1.8f);

    /* soft contact shadow */
    gfx_circle_fill(cx, cy + r + 8, (int)(r * 0.55f), RGBA(0, 0, 0, 80));

    /* main body */
    gfx_circle_fill(cx, cy, r, RGB(36, 150, 58));
    gfx_circle_fill(cx, cy + 7, r - 5, RGB(28, 115, 45));          /* volume */
    gfx_circle_fill(cx - r/3, cy - r/3, r/3, RGB(130, 230, 150));  /* highlight */
    gfx_circle_fill(cx - r/3 - 2, cy - r/3 - 2, r/6, RGB(210, 255, 220));

    /* energy rim */
    gfx_circle_outline(cx, cy, r, 3, RGBA(0, 255, 190, (int)(90 * pulse)));

    /* stem + leaf */
    gfx_rect(cx - 2, cy - r - 10, 5, 16, RGB(70, 45, 25));
    gfx_circle_fill(cx + 11, cy - r - 7, 8, RGB(35, 140, 55));
}

static void draw_nexus_windows_realistic(int cx, int cy, int size, float t, int open)
{
    float open_t = open ? fminf(1.0f, 0.25f + 0.75f * (0.5f + 0.5f * sinf(t * 2.2f))) : 0.0f;
    int s = size / 2 - 6;

    gfx_round_rect(cx - s - 6, cy - s - 6, s * 2 + 12, s * 2 + 12, 8, RGB(18, 28, 48));

    for (int i = 0; i < 4; i++) {
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float angle = side * open_t * 0.70f;

        int base_x = (i % 2 == 0) ? -s : 0;
        int base_y = (i < 2) ? -s : 0;

        int ox = (int)(base_x * cosf(angle));
        int oy = (int)(base_y + base_x * 0.12f * sinf(angle));

        gfx_round_rect(cx + ox, cy + oy, s, s, 6, RGB(0, 145, 225));
        gfx_round_rect(cx + ox + 5, cy + oy + 5, s - 12, s / 3, 4, RGBA(190, 235, 255, 130));
        gfx_round_rect_outline(cx + ox, cy + oy, s, s, 6, 2, RGB(170, 220, 255));
    }

    if (open_t > 0.35f)
        gfx_glow(cx, cy, (float)(size / 3), RGB(90, 190, 255), 0.45f * open_t);
}

static const uint32_t ice_flavors[] = {
    RGB(255, 248, 230), /* Ubuntu  – vanilla */
    RGB(65, 120, 210),  /* Fedora  – blueberry */
    RGB(95, 200, 140),  /* Arch    – mint */
    RGB(75, 45, 28),    /* Debian  – chocolate */
    RGB(155, 195, 95),  /* CachyOS – pistachio */
    RGB(235, 95, 115)   /* Nobara  – strawberry */
};
#define NUM_ICE_FLAVORS 6

static void draw_nexus_linux_icecream_realistic(int cx, int cy, int size,
                                               int current_flavor, float melt)
{
    int w = size;
    int h = (int)(size * 0.92f);
    int layer_h = (h - 24) / NUM_ICE_FLAVORS;

    gfx_circle_fill(cx, cy + h/2 + 6, (int)(w * 0.42f), RGBA(0, 0, 0, 90));

    gfx_round_rect(cx - w/2, cy - h/2, w, h, 14, RGB(28, 32, 42));
    gfx_round_rect_outline(cx - w/2, cy - h/2, w, h, 14, 3, RGB(185, 195, 215));
    gfx_round_rect(cx - w/2 - 3, cy - h/2 - 6, w + 6, 16, 7, RGB(165, 170, 180));

    for (int i = 0; i < NUM_ICE_FLAVORS; i++) {
        int ly = cy + h/2 - 20 - (i + 1) * layer_h;
        float m = (i > current_flavor) ? melt : 0.0f;
        int lh = (int)(layer_h * (1.0f - m * 0.68f));
        if (lh < 4) continue;

        gfx_round_rect(cx - w/2 + 9, ly, w - 18, lh, 5, ice_flavors[i]);
        gfx_rect(cx - w/2 + 11, ly + 2, w - 22, 4, RGBA(255, 255, 255, 70));

        if (m > 0.22f) {
            for (int d = 0; d < 3; d++) {
                int dx = cx - 28 + d * 28;
                gfx_rect(dx, ly + lh, 6, (int)(18 * m), ice_flavors[i]);
            }
        }
    }

    int sel_y = cy + h/2 - 20 - (current_flavor + 1) * layer_h;
    gfx_circle_fill(cx, sel_y - 8, 8, RGB(220, 45, 45));
    gfx_circle_fill(cx - 3, sel_y - 10, 3, RGB(255, 190, 190));
}

/* ---------- Hub State ---------- */
static int   S_selected     = 0;   /* 0=macOS 1=Windows 2=Linux */
static int   S_linux_flavor = 0;
static float S_melt         = 0.0f;
static float S_time         = 0.0f;

static void nexus_draw(Win *w);
static void nexus_select_and_boot(void);

void app_nexus_open(void)
{
    if (!wm_find(WIN_NEXUS)) {
        wm_open(WIN_NEXUS, "NEXUS // OS Selection Hub", CHROME_MAC,
                80, 40, 1760, 1000, nexus_draw);
    } else {
        wm_raise(wm_find(WIN_NEXUS));
    }
    S_time = 0.0f;
    desktop_toast("NEXUS Core Hub online");
}

static void nexus_draw(Win *w)
{
    S_time += 0.016f;

    int x = w->x, y = w->y, ww = w->w, hh = w->h;

    gfx_vgrad(x, y, ww, hh, RGB(6, 10, 18), RGB(12, 18, 32));
    gfx_round_rect_outline(x + 8, y + 8, ww - 16, hh - 16, 12, 2, RGBA(0, 200, 255, 60));

    /* header */
    gfx_round_rect(x + 20, y + 18, ww - 40, 48, 8, RGBA(10, 20, 40, 220));
    font_draw(FONT_BOLD, "NEXUS  //  OS SELECTION HUB", x + 40, y + 32, 22, RGB(0, 230, 255));
    font_draw(FONT_UI, "STATUS: STANDBY  |  LINK: SECURE  |  POWER: 100%",
              x + 520, y + 34, 16, RGB(160, 200, 220));

    int cx = x + ww / 2;
    int cy = y + hh / 2 - 20;

    gfx_circle_outline(cx, cy, 340, 3, RGBA(0, 180, 255, 70));
    gfx_circle_outline(cx, cy, 320, 2, RGBA(0, 220, 255, 50));
    gfx_glow(cx, cy, 300.0f, RGB(0, 120, 255), 0.12f);

    int icon_size = 160;
    int spacing = 280;

    /* macOS */
    draw_nexus_apple_realistic(cx - spacing, cy, icon_size, S_time);
    font_draw(FONT_BOLD, "macOS", cx - spacing - 40, cy + 110, 20,
              S_selected == 0 ? RGB(0, 255, 180) : RGB(160, 200, 220));
    if (S_selected == 0)
        gfx_circle_outline(cx - spacing, cy, icon_size/2 + 18, 3, RGB(0, 255, 200));

    /* Windows */
    draw_nexus_windows_realistic(cx, cy, icon_size, S_time, S_selected == 1);
    font_draw(FONT_BOLD, "Windows", cx - 50, cy + 110, 20,
              S_selected == 1 ? RGB(0, 200, 255) : RGB(160, 200, 220));
    if (S_selected == 1)
        gfx_circle_outline(cx, cy, icon_size/2 + 18, 3, RGB(0, 200, 255));

    /* Linux ice-cream */
    if (S_selected == 2)
        S_melt = fmaxf(0.0f, S_melt - 0.04f);
    else
        S_melt = fminf(1.0f, S_melt + 0.03f);

    draw_nexus_linux_icecream_realistic(cx + spacing, cy, icon_size, S_linux_flavor, S_melt);
    font_draw(FONT_BOLD, "Linux", cx + spacing - 35, cy + 110, 20,
              S_selected == 2 ? RGB(255, 180, 60) : RGB(160, 200, 220));
    if (S_selected == 2)
        gfx_circle_outline(cx + spacing, cy, icon_size/2 + 18, 3, RGB(255, 180, 60));

    if (S_selected == 2) {
        static const char *names[] = {
            "Ubuntu – Vanilla", "Fedora – Blueberry", "Arch – Mint",
            "Debian – Chocolate", "CachyOS – Pistachio", "Nobara – Strawberry"
        };
        font_draw(FONT_UI, names[S_linux_flavor], cx + spacing - 90, cy + 145, 16, RGB(255, 220, 140));
    }

    /* INITIATE button */
    int btn_x = cx - 140, btn_y = cy + 280, btn_w = 280, btn_h = 56;
    ui_add_hotspot(9001, btn_x, btn_y, btn_w, btn_h, "Initiate");
    int sel = (ui_sel_id() == 9001);
    gfx_round_rect(btn_x, btn_y, btn_w, btn_h, 10, sel ? RGB(0, 160, 220) : RGB(0, 100, 160));
    gfx_round_rect_outline(btn_x, btn_y, btn_w, btn_h, 10, 2, RGB(0, 220, 255));
    font_draw(FONT_BOLD, "INITIATE ENVIRONMENT", btn_x + 35, btn_y + 18, 20, RGB(255, 255, 255));

    if (ui_pressed(9001))
        nexus_select_and_boot();

    /* bottom data stream */
    gfx_round_rect(x + 20, y + hh - 70, ww - 40, 42, 8, RGBA(8, 16, 30, 230));
    font_draw(FONT_MONO, "DATA STREAM // KERNEL_NEXUS v3.7.1 | MEM_SCAN OK | QUANTUM_LINK STABLE | SIMULATION LAYER ACTIVE",
              x + 40, y + hh - 52, 15, RGB(80, 180, 200));

    /* navigation */
    if (in_clicked(BTN_LEFT))  S_selected = (S_selected + 2) % 3;
    if (in_clicked(BTN_RIGHT)) S_selected = (S_selected + 1) % 3;
    if (S_selected == 2) {
        if (in_clicked(BTN_UP))   S_linux_flavor = (S_linux_flavor + NUM_ICE_FLAVORS - 1) % NUM_ICE_FLAVORS;
        if (in_clicked(BTN_DOWN)) S_linux_flavor = (S_linux_flavor + 1) % NUM_ICE_FLAVORS;
    }
    if (in_clicked(BTN_CIRCLE) || in_clicked(BTN_OPTIONS)) {
        wm_close(WIN_NEXUS);
    }
}

static void nexus_select_and_boot(void)
{
    switch (S_selected) {
    case 0:
        desktop_set_os_target(OS_TARGET_MACOS);
        desktop_toast("NEXUS \u2192 macOS Aqua Slate online");
        break;
    case 1:
        desktop_set_os_target(OS_TARGET_WINDOWS);
        desktop_toast("NEXUS \u2192 Windows Fluent Core online");
        break;
    case 2:
        desktop_set_os_target(OS_TARGET_LINUX);
        desktop_set_linux_flavor(S_linux_flavor % 2);
        desktop_toast("NEXUS \u2192 Linux environment online");
        break;
    }
    wm_close(WIN_NEXUS);
}
