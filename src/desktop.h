/* desktop.h - desktop shell (wallpaper, menubar, dock, dialogs) */
#ifndef DESKTOP_H
#define DESKTOP_H
#include "common.h"

enum { DS_BOOT = 0, DS_RUN, DS_SLEEP, DS_SHUTDOWN };

void desktop_init(void);
void desktop_frame(void);                 /* one full frame */
int  desktop_state(void);
void desktop_open_app(int app);
void desktop_toast(const char *fmt, ...);
void desktop_begin_shutdown(void);
void desktop_draw_wallpaper(void);

#endif
