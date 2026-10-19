/* desktop.h - desktop shell (wallpaper, menubar, dock, dialogs) */
#ifndef DESKTOP_H
#define DESKTOP_H
#include "common.h"

enum { DS_BOOT = 0, DS_RUN, DS_SLEEP, DS_SHUTDOWN };

/* Boot Camp OS-shell targets driving the desktop background router */
enum { OS_TARGET_MACOS = 0, OS_TARGET_WINDOWS = 1, OS_TARGET_LINUX = 2 };

void desktop_init(void);
void desktop_frame(void);                 /* one full frame */
int  desktop_state(void);
void desktop_open_app(int app);
void desktop_toast(const char *fmt, ...);
void desktop_begin_shutdown(void);
void desktop_draw_wallpaper(void);

/* OS theme router: the whole background canvas re-styles to the target
 * environment in a single frame (no transition pass, zero lag).
 * 0 = macOS Aqua Slate, 1 = Windows Fluent Core, 2 = Linux Wayland Matrix. */
void desktop_set_os_target(int os);
int  desktop_os_target(void);
void desktop_render_background(int active_os_target, int screen_width, int screen_height);

#endif
