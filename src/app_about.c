/* app_about.c - "About This Mac" + System Report */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "sysinfo.h"
#include "common.h"

static SysInfo S_si;
static int S_report;      /* 0 = about page, 1 = system report */

static void about_draw(Win *w);
static void telemetry_draw(Win *w);

void app_about_open(void)
{
    sysinfo_collect(&S_si);
    S_report = 0;
    if (!wm_find(WIN_ABOUT))
        wm_open(WIN_ABOUT, "About This Mac", CHROME_MAC, 480, 150, 960, 660, about_draw);
    else
        wm_raise(wm_find(WIN_ABOUT));
}

/* telemetry window: the widget owns its state via win->user_data */
static void telemetry_draw(Win *w)
{
    sysinfo_render_telemetry_widget(w);
}

static void telemetry_open(void)
{
    if (!wm_find(WIN_TELEMETRY))
        wm_open(WIN_TELEMETRY, "Cluster Telemetry", CHROME_MAC, 640, 320, 560, 310,
                telemetry_draw);
    else
        wm_raise(wm_find(WIN_TELEMETRY));
}

static void row(Win *w, int *yy, const char *k, const char *v)
{
    font_draw(FONT_UI, k, w->x + 400, *yy, 18, g_theme.text_dim);
    font_draw(FONT_UI, v, w->x + 560, *yy, 18, g_theme.text);
    *yy += 40;
}

static void about_draw(Win *w)
{
    if (!S_report) {
        /* hero */
        icons_draw_apple(w->x + 200, w->y + 160, 110.f, g_theme.text_bright);
        font_draw(FONT_BOLD, "PS5GreenApple", w->x + 90, w->y + 250, 30, g_theme.text_bright);
        font_draw(FONT_UI, "macOS Sonoma-style desktop for PS5", w->x + 66, w->y + 292, 17, g_theme.text_dim);
        font_draw(FONT_UI, "Version " PSA_VERSION " (SDL2)", w->x + 98, w->y + 316, 17, g_theme.text_dim);

        int yy = w->y + 390;
        row(w, &yy, "Chip", S_si.cpu);
        row(w, &yy, "Memory", S_si.mem_total);
        row(w, &yy, "Firmware", S_si.fw);
        row(w, &yy, "Loader", S_si.loader);
        row(w, &yy, "Display", S_si.display);

        if (ui_button(500, w->x + w->w / 2 - 280, w->y + w->h - 92, 260, 50,
                      "System Report...", 0)) { }
        if (ui_pressed(500)) S_report = 1;
        if (ui_button(502, w->x + w->w / 2 + 20, w->y + w->h - 92, 260, 50,
                      "Cluster Telemetry", 0)) { }
        if (ui_pressed(502)) telemetry_open();
    } else {
        font_draw(FONT_BOLD, "System Report", w->x + 32, w->y + 66, 26, g_theme.text_bright);
        int yy = w->y + 120;
        row(w, &yy, "Model", S_si.model);
        row(w, &yy, "CPU", S_si.cpu);
        row(w, &yy, "Total Cores", S_si.cpu_cores);
        row(w, &yy, "Memory", S_si.mem_total);
        row(w, &yy, "Kernel", S_si.kernel);
        row(w, &yy, "OS Release", S_si.fw);
        row(w, &yy, "Renderer", S_si.renderer);
        row(w, &yy, "Display", S_si.display);

        char up[32];
        sysinfo_uptime_str(up, sizeof(up));
        row(w, &yy, "Session Uptime", up);

        /* storage bar */
        char dtitle[64];
        psa_snprintf(dtitle, sizeof(dtitle), "Storage: %ld MB total, %ld MB free",
                     S_si.disk_total_mb, S_si.disk_free_mb);
        font_draw(FONT_UI, dtitle, w->x + 400, yy + 6, 18, g_theme.text_dim);
        int bw = w->w - 460;
        float used = 1.f - (float)S_si.disk_free_mb / (float)(S_si.disk_total_mb > 0 ? S_si.disk_total_mb : 1);
        gfx_round_rect(w->x + 400, yy + 36, bw, 14, 7, RGB(58, 58, 64));
        gfx_round_rect(w->x + 400, yy + 36, (int)(bw * clampf(used, 0.02f, 1.f)), 14, 7, g_theme.accent);

        if (ui_button(501, w->x + w->w / 2 - 110, w->y + w->h - 92, 220, 50, "Back", 0)) { }
        if (ui_pressed(501)) S_report = 0;
    }
}
