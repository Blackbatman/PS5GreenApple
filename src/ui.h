/* ui.h - hotspot list + D-pad focus navigation (PS5-style selection cursor) */
#ifndef UI_H
#define UI_H
#include "common.h"

#define UI_MAX_HOTSPOTS 160

typedef struct Hotspot {
    int id, x, y, w, h;
    char label[48];
} Hotspot;

void ui_begin_frame(void);
void ui_add_hotspot(int id, int x, int y, int w, int h, const char *label);
int  ui_hotspot_count(void);
Hotspot *ui_hotspot_at(int i);

int  ui_sel_id(void);                 /* current selected hotspot id (0 = none) */
void ui_set_sel(int id);
void ui_nav(int dir);                 /* 0=up 1=down 2=left 3=right */
int  ui_pressed(int id);              /* selected == id && cross clicked */
int  ui_pressed_btn(int id, int btn); /* selected == id && btn clicked */
void ui_sel_rect(int *x, int *y, int *w, int *h);

/* shared widget helpers */
int  ui_button(int id, int x, int y, int w, int h, const char *label, int primary);

#endif
