/* icons.h - procedural vector icons */
#ifndef ICONS_H
#define ICONS_H
#include "common.h"

typedef enum {
    ICON_APPLE = 0, ICON_FINDER, ICON_TERMINAL, ICON_TEXTEDIT, ICON_ABOUT,
    ICON_SETTINGS, ICON_BOOTCAMP, ICON_TRASH, ICON_FOLDER, ICON_FILE, ICON_IMAGE,
    ICON_WIFI, ICON_BATTERY, ICON_CC, ICON_SEARCH, ICON_WIN, ICON_LINUX,
    ICON_GEAR, ICON_CHEVRON_R, ICON_DOT
} IconKind;

/* Apple logo (simple-icons path, flattened beziers) */
void icons_draw_apple(int cx, int cy, float size, uint32_t c);

/* Dock-style rounded-square app icon */
void icons_draw_app_icon(IconKind kind, int x, int y, int size, int selected);
/* small glyph (monochrome) */
void icons_draw_glyph(IconKind kind, int cx, int cy, int size, uint32_t c);

#endif
