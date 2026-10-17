/* theme.h - Sonoma dark palette */
#ifndef THEME_H
#define THEME_H
#include "common.h"

typedef struct Theme {
    uint32_t wall_top, wall_bot;
    uint32_t menubar_bg, menubar_text;
    uint32_t window_bg, window_border, titlebar_bg, titlebar_text;
    uint32_t text, text_dim, text_bright;
    uint32_t accent;
    uint32_t dock_bg, dock_border;
    uint32_t sel_bg, sel_text;
    uint32_t green, yellow, red;
    uint32_t sidebar_bg, content_bg, row_hover, separator;
    uint32_t input_bg;
} Theme;

extern Theme g_theme;
void theme_init(void);
const char *accent_name(void);
int  accent_count(void);
void accent_set(int idx);

#endif
