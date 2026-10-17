/* app_settings.c - settings with persistence to PSA_DATA_DIR/settings.ini */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "settings.h"
#include "desktop.h"
#include "common.h"

static void settings_draw(Win *w);

void app_settings_open(void)
{
    if (!wm_find(WIN_SETTINGS))
        wm_open(WIN_SETTINGS, "System Settings", CHROME_MAC, 470, 130, 980, 700, settings_draw);
    else
        wm_raise(wm_find(WIN_SETTINGS));
}

static void toggle_row(Win *w, int id, const char *label, const char *value, int *yy)
{
    int x = w->x + 30, cw = w->w - 60;
    int rh = 56;
    int sel = (ui_sel_id() == id);
    if (sel) gfx_round_rect(x, *yy, cw, rh, 10, g_theme.row_hover);
    font_draw(FONT_UI, label, x + 18, *yy + 16, 19, g_theme.text);
    font_draw(FONT_UI, value, x + cw - font_text_w(FONT_UI, value, 18) - 18, *yy + 16, 18,
              sel ? g_theme.accent : g_theme.text_dim);
    ui_add_hotspot(id, x, *yy, cw, rh, label);
    *yy += rh + 8;
    if (ui_pressed(id)) settings_save();
}

static void settings_draw(Win *w)
{
    font_draw(FONT_BOLD, "Appearance", w->x + 32, w->y + 70, 22, g_theme.text_bright);
    int yy = w->y + 108;

    char buf[48];
    psa_snprintf(buf, sizeof(buf), "%s >", wallpaper_name(g_settings.wallpaper));
    toggle_row(w, 600, "Wallpaper", buf, &yy);
    if (ui_pressed(600)) {
        g_settings.wallpaper = (g_settings.wallpaper + 1) % wallpaper_count();
        settings_save();
    }
    psa_snprintf(buf, sizeof(buf), "%s >", accent_name());
    toggle_row(w, 601, "Accent color", buf, &yy);
    if (ui_pressed(601)) {
        g_settings.accent = (g_settings.accent + 1) % accent_count();
        theme_init();
        settings_save();
    }

    font_draw(FONT_BOLD, "Menu Bar & Clock", w->x + 32, yy + 14, 22, g_theme.text_bright);
    yy += 56;
    toggle_row(w, 602, "24-Hour Time", g_settings.clock24 ? "On" : "Off", &yy);
    if (ui_pressed(602)) { g_settings.clock24 ^= 1; settings_save(); }
    toggle_row(w, 603, "Show Seconds", g_settings.seconds ? "On" : "Off", &yy);
    if (ui_pressed(603)) { g_settings.seconds ^= 1; settings_save(); }

    font_draw(FONT_BOLD, "Advanced", w->x + 32, yy + 14, 22, g_theme.text_bright);
    yy += 56;
    toggle_row(w, 604, "FPS Overlay", g_settings.fps ? "On" : "Off", &yy);
    if (ui_pressed(604)) { g_settings.fps ^= 1; settings_save(); }

    char p[128];
    psa_snprintf(p, sizeof(p), "Settings persist to %s/settings.ini", PSA_DATA_DIR);
    font_draw(FONT_UI, p, w->x + 32, w->y + w->h - 46, 15, g_theme.text_dim);
}
