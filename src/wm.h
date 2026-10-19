/* wm.h - window manager with per-OS chrome styles (macOS / Windows / Linux) */
#ifndef WM_H
#define WM_H
#include "common.h"

typedef enum { CHROME_MAC = 0, CHROME_WIN = 1, CHROME_LINUX = 2 } ChromeStyle;

typedef struct Win Win;
typedef void (*WinDraw)(Win *w);

struct Win {
    int id, open;
    int x, y, w, h;
    char title[72];
    ChromeStyle chrome;
    int ws;                 /* workspace id (future: multiple desktops) */
    WinDraw draw;
    int scroll_off;         /* content scroll helper for apps */
    int modal;
    /* app-owned state slot: apps may lazy-init a private struct here.
     * The window manager owns the disposal: when the window closes,
     * user_free(user_data) runs exactly once (if set), then both are
     * cleared - no leaks, no dangling pointers across reopen cycles. */
    void *user_data;
    void (*user_free)(void *user_data);
};

#define WM_MAX_WINDOWS 12

/* window ids */
enum {
    WIN_FINDER = 1, WIN_TERMINAL, WIN_TEXTEDIT, WIN_ABOUT, WIN_SETTINGS,
    WIN_BOOTCAMP, WIN_BC_WIN, WIN_BC_LINUX, WIN_POWER, WIN_TELEMETRY
};

void wm_init(void);
Win *wm_open(int id, const char *title, ChromeStyle ch, int x, int y, int w, int h,
             WinDraw draw);
void wm_close(int id);
void wm_close_top(void);
Win *wm_find(int id);
Win *wm_top(void);
void wm_raise(Win *w);
void wm_draw_all(void);
int  wm_any_open(void);
/* titlebar hotspot ids for close buttons: 60 + win id */
void wm_hit_test_close(int mx, int my); /* unused hook (future mouse) */

#endif
