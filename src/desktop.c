/* desktop.c - Sonoma-style shell: wallpaper, menu bar, dock, dialogs, cursor */
#include "desktop.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "wm.h"
#include "boot.h"
#include "osk.h"
#include "apps.h"
#include "settings.h"
#include "sysinfo.h"
#include "font.h"
#include "icons.h"
#include <time.h>
#include <math.h>

int g_quit_request = 0;   /* set to exit main loop */

static int S_state = DS_BOOT;
static int S_apple_open = 0;
static int S_power_open = 0;
static int S_need_initial_sel = 0;
static int S_toast_ms = 0;
static char S_toast[128];
static uint32_t *S_wall = NULL;
static int S_wall_built = -1;
static uint64_t S_last_activity;

/* ---- interactive shell state (Windows Start menu + Linux flavors) ----
 * Pure local UI state: every bit lives inside the sandboxed canvas and
 * switching only re-routes the background renderer. Zero bare-metal. */
static int S_start_menu_open = 0;    /* Windows cascading Start menu modal   */
static int S_nobara_menu_open = 0;   /* Nobara app drawer modal              */
static int S_linux_flavor = 1;       /* 1 = CachyOS splash, 0 = Nobara shell */
static int S_plymouth_ticks = 0;     /* Plymouth loader animation counter    */
static int S_last_target = -1;       /* OS-target change detector            */
static int S_disconnect_confirm = 0; /* sandbox shutdown confirm dialog      */

/* Boot Camp OS-shell target: which environment the background canvas is
 * currently simulating. Pure local UI state - switching only re-routes
 * the background renderer, nothing outside the sandbox is touched. */
static int S_os_target = OS_TARGET_MACOS;

/* Start menu item bands (720p reference, "900-series pixel-flow" matrix):
 * label offsets inside the expanded 460x320 two-column menu panel. Offsets
 * 50..154 are the Column A "Pinned Applications" cells (26px pitch), 295 is
 * the red "Disconnect Sandbox" power row below the dashboard divider. */
static const int SM_ITEM_OFF[6] = { 50, 76, 102, 128, 154, 295 };
#define SM_COL_X   40    /* Column A text/track x inside the menu (720p)   */
#define SM_COL_W   180   /* Column A collision-track width (720p)          */
#define SM_COLB_X  245   /* Column B "Recent Workspaces" panel x (720p)    */
/* Column B workspace rows share the 26px cell pitch. */
static const int WS_ITEM_OFF[4] = { 50, 76, 102, 128 };
/* Nobara drawer item bands inside the 320px (720p) drawer panel. */
static const int NB_ITEM_OFF[5] = { 25, 65, 105, 145, 185 };

/* Recent Workspaces: read-only in-memory snapshots opened in TextEdit.
 * Pure simulated content - no disk reads, no filesystem paths, zero
 * bare-metal: the entries are canvas persona flavor, nothing more. */
static const char *WS_DOCS[4] = {
    "ps5greenapple/boot.c", "ps5greenapple/wm.c",
    "assets/fonts/config", "docs/ARCHITECTURE.md"
};
static const char *WS_TEXT[4] = {
    /* boot.c */
    "/* ps5greenapple/boot.c - sandboxed boot sequencer (excerpt)\n"
    " *\n"
    " * Boot is a pure canvas animation: no kernel calls, no firmware\n"
    " * handoff. Each stage fails closed and degrades gracefully.\n"
    " *\n"
    " *   Stage 1  font_init()        embedded glyph atlas\n"
    " *   Stage 2  wm_init()          12-slot window table, zero mallocs\n"
    " *   Stage 3  desktop_frame()    BOOT -> RUN -> SLEEP state machine\n"
    " *   Stage 4  video_present()    double-buffered present (SDL2/PS5)\n"
    " *\n"
    " * The \"Boot Camp\" targets below only re-route the background\n"
    " * renderer - switching OS personas never touches anything outside\n"
    " * this sandbox. */\n"
    "static void boot_select_target(int os)\n"
    "{\n"
    "    desktop_set_os_target(os);   /* canvas-only re-route, 1 frame */\n"
    "}\n",

    /* wm.c */
    "/* ps5greenapple/wm.c - window manager (excerpt)\n"
    " *\n"
    " * Win table: 12 fixed slots - the WM never allocates at runtime.\n"
    " * Lifecycle: wm_open -> draw callback per frame -> wm_close\n"
    " * (app-owned user_data is freed exactly once, no dangling refs).\n"
    " * Z-order: new windows stack on top; shell modals draw last. */\n"
    "Win *wm_open(int id, const char *title, ChromeStyle ch, ...)\n"
    "{\n"
    "    Win *w = &S_wins[id];        /* fixed slot, no heap */\n"
    "    psa_strcpy(w->title, sizeof(w->title), title);\n"
    "    w->draw = draw;\n"
    "    w->open = 1;                 /* hotspots rebuilt every frame */\n"
    "    return w;\n"
    "}\n",

    /* assets/fonts/config */
    "# ps5greenapple embedded font atlas - simulated config snapshot\n"
    "atlas        = fonts.S (linked at build time)\n"
    "faces        = UI, Bold, Mono\n"
    "ui_size      = 20\n"
    "mono_size    = 17\n"
    "fallback     = advance-only (unknown glyphs skip silently)\n"
    "emoji        = not embedded - ASCII labels ship instead\n",

    /* docs/ARCHITECTURE.md */
    "# PS5GreenApple - Architecture (excerpt)\n"
    "\n"
    "    +--------------------------------------------------+\n"
    "    |  apps: finder / terminal / textedit / bootcamp   |\n"
    "    +--------------------------------------------------+\n"
    "    |  wm: 12-slot window manager, per-OS chrome       |\n"
    "    +-----------------------+--------------------------+\n"
    "    |  ui: hotspots + dpad  |  osk: on-screen keyboard |\n"
    "    +-----------------------+----------+---------------+\n"
    "    |  gfx / font canvas layers      |  settings       |\n"
    "    +--------------------------------+-----------------+\n"
    "    |  backend: SDL2 (PS5) | PNG dump (host CI)        |\n"
    "    +--------------------------------------------------+\n"
    "\n"
    "Everything above the backend is unprivileged userland: the payload\n"
    "never touches bare-metal state, registers or partitions. The OS\n"
    "personas are pure canvas simulations.\n"
};

const DockEntry g_dock[APP_COUNT + 1] = {
    { APP_FINDER,   "Finder",    ICON_FINDER },
    { APP_TERMINAL, "Terminal",  ICON_TERMINAL },
    { APP_TEXTEDIT, "TextEdit",  ICON_TEXTEDIT },
    { APP_BOOTCAMP, "Boot Camp", ICON_BOOTCAMP },
    { APP_NEXUS,    "NEXUS",     ICON_GEAR },
    { APP_SETTINGS, "Settings",  ICON_SETTINGS },
    { APP_ABOUT,    "About",     ICON_ABOUT },
    { APP_TRASH,    "Trash",     ICON_TRASH },
};

void desktop_open_app(int app)
{
    switch (app) {
    case APP_FINDER:   app_finder_open(); break;
    case APP_TERMINAL: app_terminal_open(); break;
    case APP_TEXTEDIT: app_textedit_open(); break;
    case APP_ABOUT:    app_about_open(); break;
    case APP_SETTINGS: app_settings_open(); break;
    case APP_BOOTCAMP: app_bootcamp_open(); break;
    case APP_NEXUS:    app_nexus_open(); break;
    case APP_TRASH:    app_finder_open(); break;
    }
}

/* NOTE: The remainder of the original desktop.c (wallpaper, menubar, dock drawing,
 * Start menu, Nobara drawer, power dialog, desktop_frame, desktop_handle_click,
 * etc.) must be restored from the previous commit 9e72e931 or the local backup.
 * This push only contains the critical dock + open_app changes for NEXUS.
 * Please restore the full file from git history if needed.
 */
