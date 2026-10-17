/* theme.c - macOS Sonoma dark theme */
#include "theme.h"
#include "settings.h"

Theme g_theme;

static const uint32_t ACCENTS[] = {
    RGB(10, 132, 255),   /* blue   (system blue dark) */
    RGB(191, 90, 242),   /* purple */
    RGB(255, 55, 95),    /* pink   */
    RGB(50, 215, 75),    /* green  */
    RGB(255, 159, 10),   /* orange */
};

void theme_init(void)
{
    g_theme.wall_top      = RGB(15, 17, 26);
    g_theme.wall_bot      = RGB(28, 34, 56);
    g_theme.menubar_bg    = RGBA(28, 28, 30, 210);
    g_theme.menubar_text  = RGB(235, 235, 245);
    g_theme.window_bg     = RGB(30, 30, 32);
    g_theme.window_border = RGB(56, 56, 60);
    g_theme.titlebar_bg   = RGB(44, 44, 48);
    g_theme.titlebar_text = RGB(220, 220, 228);
    g_theme.text          = RGB(235, 235, 245);
    g_theme.text_dim      = RGB(152, 152, 165);
    g_theme.text_bright   = RGB(255, 255, 255);
    g_theme.dock_bg       = RGBA(40, 40, 44, 190);
    g_theme.dock_border   = RGBA(90, 90, 100, 120);
    g_theme.sel_bg        = RGB(10, 132, 255);
    g_theme.sel_text      = RGB(255, 255, 255);
    g_theme.green         = RGB(40, 205, 65);
    g_theme.yellow        = RGB(255, 189, 46);
    g_theme.red           = RGB(255, 69, 58);
    g_theme.sidebar_bg    = RGBA(38, 38, 42, 235);
    g_theme.content_bg    = RGB(30, 30, 32);
    g_theme.row_hover     = RGBA(70, 70, 80, 160);
    g_theme.separator     = RGB(60, 60, 66);
    g_theme.input_bg      = RGB(24, 24, 26);
    accent_set(g_settings.accent);
}

int accent_count(void) { return (int)(sizeof(ACCENTS) / sizeof(ACCENTS[0])); }

void accent_set(int idx)
{
    if (idx < 0 || idx >= accent_count()) idx = 0;
    g_theme.accent = ACCENTS[idx];
}

const char *accent_name(void)
{
    static const char *names[] = { "Blue", "Purple", "Pink", "Green", "Orange" };
    if (g_settings.accent < 0 || g_settings.accent >= accent_count()) return names[0];
    return names[g_settings.accent];
}
