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

/* Windows persona: the Start button toggles an expanded two-column cascading
 * popup rendered as a global modal above the window layer. Column A pins the
 * existing apps (System Specs / File Explorer / Command Prompt / Notepad /
 * Boot Camp), Column B mirrors recent workspaces, and the lower dashboard
 * row carries a safe, sandboxed shutdown sequence ("Disconnect Sandbox"). */
void desktop_toggle_start_menu(void);

/* Linux persona is dual-flavor: 1 = CachyOS Plymouth boot splash (animated),
 * 0 = Nobara GNOME shell with its own app drawer. Entering Linux always
 * lands on the splash first, like a real distro boot. */
void desktop_toggle_nobara_menu(void);
void desktop_set_linux_flavor(int flavor);
int  desktop_get_linux_flavor(void);

/* Pointer click router (DualSense touchpad tap on PS5, host mouse):
 * coordinate hit-test against the shell chrome - Start button, cascading
 * menu items, Plymouth buttons, Nobara drawer - then against any hotspot
 * under the pointer. Returns 1 when the click was consumed. */
int  desktop_handle_click(int mx, int my);

#endif
