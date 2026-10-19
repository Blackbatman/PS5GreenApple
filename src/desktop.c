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
    case APP_TRASH:    app_finder_open(); break;
    }
}

void desktop_toast(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(S_toast, sizeof(S_toast), fmt, ap);
    va_end(ap);
    S_toast_ms = (int)now_ms();
}

void desktop_begin_shutdown(void) { S_state = DS_SHUTDOWN; }
int  desktop_state(void) { return S_state; }

void desktop_set_os_target(int os)
{
    /* defensive: unknown targets fall back to the macOS shell */
    if (os < OS_TARGET_MACOS || os > OS_TARGET_LINUX) os = OS_TARGET_MACOS;
    if (os == S_os_target) return;
    S_os_target = os;
    /* switching environments closes every popup instantly and re-enters
     * Linux through the Plymouth splash (authentic boot flow) */
    S_start_menu_open = 0;
    S_nobara_menu_open = 0;
    S_disconnect_confirm = 0;
    if (os == OS_TARGET_LINUX) { S_linux_flavor = 1; S_plymouth_ticks = 0; }
}

int desktop_os_target(void) { return S_os_target; }

/* ---------------- shell modal controls ---------------- */
void desktop_toggle_start_menu(void)
{
    S_start_menu_open = !S_start_menu_open;
    if (S_start_menu_open) S_apple_open = 0;   /* modal hygiene: one popup at a time */
}

void desktop_toggle_nobara_menu(void)
{
    S_nobara_menu_open = !S_nobara_menu_open;
}

void desktop_set_linux_flavor(int flavor)
{
    /* defensive: anything but the explicit Nobara shell falls back to the
     * Plymouth splash, which is the safe entry sub-mode */
    S_linux_flavor = (flavor == 0) ? 0 : 1;
    S_nobara_menu_open = 0;
    if (S_linux_flavor == 1) S_plymouth_ticks = 0;
}

int desktop_get_linux_flavor(void) { return S_linux_flavor; }

/* ---------------- shared shell geometry (720p reference, scaled) --------
 * Single source of truth so the renderer, the per-frame hotspots and the
 * pointer hit-test router can never drift apart. */
typedef struct {
    float s;                        /* 720p -> canvas scale factor */
    /* Windows taskbar + cascading Start menu */
    int tb_y, tb_h;
    int st_x, st_y, st_w, st_h;     /* Start button */
    int bc_x, bc_y, bc_w, bc_h;     /* taskbar Boot Camp tray button */
    int mn_x, mn_y, mn_w, mn_h;     /* Start menu panel */
    /* CachyOS Plymouth splash */
    int pl_x, pl_y, pl_w, pl_h;     /* Load Nobara GUI button */
    int pl_bm_y;                    /* Boot Manager button y */
    /* Nobara GNOME shell */
    int nb_panel_h;
    int nb_apps_x, nb_apps_y, nb_apps_w, nb_apps_h;
    int nb_sw_x, nb_sw_w;           /* CachyOS Splash switcher */
    int nb_dr_x, nb_dr_y, nb_dr_w, nb_dr_h;
} ShellGeo;

static void shell_geo(int sw, int sh, ShellGeo *g)
{
    if (!g) return;                 /* defensive: null geometry sink */
    memset(g, 0, sizeof(*g));
    float s = (float)sh / 720.f;
    if (s < 0.75f) s = 0.75f;       /* same clamps as the background router */
    if (s > 2.5f) s = 2.5f;
    g->s = s;

    /* Advanced taskbar matrix: sleek 40px bar, 80x32 Start cell inset 4px
     * from the bar edges (perfect 900-series pixel-flow coordinates) */
    g->tb_h = (int)(40 * s);  g->tb_y = sh - g->tb_h;
    g->st_w = (int)(80 * s);  g->st_h = (int)(32 * s);
    g->st_x = (int)(5 * s);   g->st_y = sh - (int)(36 * s);
    g->bc_w = (int)(64 * s);  g->bc_h = (int)(32 * s);
    g->bc_x = sw - g->bc_w - (int)(150 * s);  g->bc_y = g->st_y;
    /* expanded two-column cascading menu housing */
    g->mn_w = (int)(460 * s); g->mn_h = (int)(320 * s);
    g->mn_x = (int)(5 * s);   g->mn_y = g->tb_y - g->mn_h;

    g->pl_w = (int)(280 * s); g->pl_h = (int)(40 * s);
    g->pl_x = sw / 2 - g->pl_w / 2;
    g->pl_y = sh / 2 + (int)(100 * s);
    g->pl_bm_y = g->pl_y + g->pl_h + (int)(14 * s);

    g->nb_panel_h = (int)(35 * s);
    g->nb_apps_w = (int)(110 * s); g->nb_apps_h = (int)(26 * s);
    g->nb_apps_x = (int)(5 * s);   g->nb_apps_y = (int)(4 * s);
    g->nb_sw_w = (int)(120 * s);
    g->nb_sw_x = sw - (int)(280 * s);
    g->nb_dr_w = (int)(260 * s);   g->nb_dr_h = (int)(320 * s);
    g->nb_dr_x = (int)(5 * s);     g->nb_dr_y = g->nb_panel_h + (int)(3 * s);
}

/* pointer hover (host mouse / DualSense touchpad): pure coordinate math
 * against the item's own collision rect - no list walks, zero overhead */
static int ptr_in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* flat shell button (Fluent/GNOME look): flat body, centered label, hotspot.
 * Highlights on dpad focus OR pointer hover (touchpad finger / host mouse). */
static void shell_button(int id, int x, int y, int w, int h,
                         const char *label, uint32_t bg, uint32_t hl)
{
    if (w <= 0 || h <= 0 || !label) return;   /* defensive geometry */
    int sel = (ui_sel_id() == id);
    int hov = !sel && ptr_in_rect(g_in.mx, g_in.my, x, y, w, h);
    gfx_round_rect(x, y, w, h, 8, (sel || hov) ? hl : bg);
    if (sel) gfx_round_rect_outline(x - 4, y - 4, w + 8, h + 8, 10, 2, RGB(255, 255, 255));
    int tw = font_text_w(FONT_UI, label, 19);
    font_draw(FONT_UI, label, x + (w - tw) / 2, y + (h - 24) / 2, 19, RGB(255, 255, 255));
    ui_add_hotspot(id, x, y, w, h, label);
}

/* GNOME-style panel clock: weekday + 24h time */
static void panel_clock(char *buf, int n)
{
    if (!buf || n <= 0) return;
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    static const char *wd[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    psa_snprintf(buf, (size_t)n, "%s %02d:%02d",
                 wd[tmv.tm_wday % 7], tmv.tm_hour & 0x1F, tmv.tm_min);
}

static void draw_clock(char *buf, int n);   /* forward: defined below */

/* ---------------- OS background router (Boot Camp sync) ----------------
 * The desktop background canvas instantly re-styles to match the OS
 * selected in Boot Camp, in a single frame: the switch is a pure render
 * re-route with no transition pass and no allocation.
 * Geometry is the 1280x720 reference layout scaled to the actual canvas.
 * Colors go through RGB() because the pipeline is 0xAARRGGBB. */
void desktop_render_background(int active_os_target, int screen_width, int screen_height)
{
    int sw = screen_width, sh = screen_height;
    if (sw < 320) sw = 320;              /* defensive lower bounds */
    if (sh < 240) sh = 240;
    float s = (float)sh / 720.f;         /* reference sketch was 720p */
    if (s < 0.75f) s = 0.75f;
    if (s > 2.5f) s = 2.5f;

    /* modal-state guard: switching environments closes every popup instantly
     * and always re-enters Linux through the Plymouth splash */
    if (active_os_target != S_last_target) {
        S_start_menu_open = 0;
        S_nobara_menu_open = 0;
        if (active_os_target == OS_TARGET_LINUX) { S_linux_flavor = 1; S_plymouth_ticks = 0; }
        S_last_target = active_os_target;
    }

    switch (active_os_target) {
    case OS_TARGET_MACOS:
        /* macOS Workstation: Deep Premium Apple Obsidian theme (Aqua Slate).
         * Standalone fallback strip - the live desktop normally layers the
         * Sonoma wallpaper + full menubar on top of this route. */
        gfx_clear(RGB(0x16, 0x16, 0x17));
        gfx_rect(0, 0, sw, (int)(40 * s), RGB(0x24, 0x24, 0x26));   /* menu bar */
        font_draw_string(15, 12, "Finder  File  Edit  View  Go  Window  Help",
                         RGB(0xFF, 0xFF, 0xFF));
        break;

    case OS_TARGET_WINDOWS: {
        /* Windows: Classic Midnight Teal Workstation layout (Fluent Core),
         * obsidian-charcoal taskbar & spacing refinement. The Start button
         * drives the cascading menu modal (drawn above the window layer by
         * draw_start_menu) - zero-lag state toggling. */
        ShellGeo gg;
        shell_geo(sw, sh, &gg);

        gfx_clear(RGB(0x00, 0x5A, 0x5B));   /* classic mid-teal workstation wall */
        gfx_rect(0, gg.tb_y, sw, gg.tb_h, RGB(0x10, 0x10, 0x12));    /* sleek taskbar */
        gfx_rect(0, gg.tb_y, sw, 1, RGB(0x2C, 0x2C, 0x2E));          /* high-contrast accent border */

        /* Start button: hover lift + pressed look while the modal is open */
        int st_hov = !S_start_menu_open &&
                     ptr_in_rect(g_in.mx, g_in.my, gg.st_x, gg.st_y, gg.st_w, gg.st_h);
        gfx_rect(gg.st_x, gg.st_y, gg.st_w, gg.st_h,
                 S_start_menu_open ? RGB(0x2C, 0x2C, 0x2E)
                                   : (st_hov ? RGB(0x2C, 0x2C, 0x2E)
                                             : RGB(0x1C, 0x1C, 0x1E)));
        font_draw_string(gg.st_x + (int)(13 * gg.s),
                         gg.st_y + (gg.st_h - 22) / 2 + 1,
                         "Start", RGB(0xFF, 0xFF, 0xFF));
        ui_add_hotspot(900, gg.st_x, gg.st_y, gg.st_w, gg.st_h, "start-menu");

        /* system tray: live taskbar clock + permanent Boot Camp escape hatch
         * so the boot manager (and from it, the macOS shell) is always one
         * click away while the Windows persona is active */
        char clk[16];
        draw_clock(clk, sizeof(clk));
        int cw = font_text_w(FONT_UI, clk, 18);
        font_draw(FONT_UI, clk, sw - cw - (int)(16 * gg.s),
                  gg.tb_y + (gg.tb_h - 22) / 2, 18, RGB(0x8E, 0x8E, 0x93));
        int bc_hov = ptr_in_rect(g_in.mx, g_in.my, gg.bc_x, gg.bc_y, gg.bc_w, gg.bc_h);
        gfx_round_rect(gg.bc_x, gg.bc_y, gg.bc_w, gg.bc_h, 6,
                       bc_hov ? RGB(0x3A, 0x3A, 0x3C) : RGB(0x2C, 0x2C, 0x2E));
        font_draw(FONT_UI, "BC", gg.bc_x + (gg.bc_w - font_text_w(FONT_UI, "BC", 18)) / 2,
                  gg.bc_y + (gg.bc_h - 22) / 2, 18, RGB(0x9A, 0x9A, 0xA0));
        ui_add_hotspot(907, gg.bc_x, gg.bc_y, gg.bc_w, gg.bc_h, "taskbar-bootcamp");
        break;
    }

    case OS_TARGET_LINUX:
        if (desktop_get_linux_flavor() == 1) {
            /* -- SUB-MODE A: CachyOS / Plymouth boot splash simulation --
             * Animated on the core render clock: the light-green tracker
             * sweeps the loading track, then the user enters the live
             * Nobara shell with one click. Pure canvas state, no kernel. */
            ShellGeo gg;
            shell_geo(sw, sh, &gg);
            gfx_clear(RGB(0x0C, 0x0C, 0x0E));   /* deep pitch-black stealth screen */

            S_plymouth_ticks += 2;
            if (S_plymouth_ticks >= 420) S_plymouth_ticks = 0;   /* 420 % 42 == 0: seamless wrap */
            int track_w = (int)(170 * gg.s), track_h = (int)(6 * gg.s);
            int seg_w   = (int)(36 * gg.s);
            int tx0 = sw / 2 - track_w / 2, ty0 = sh / 2 - (int)(20 * gg.s);
            int off = (S_plymouth_ticks % 42) * (track_w - seg_w) / 42;
            gfx_rect(tx0, ty0, track_w, track_h, RGB(0x1C, 0x1C, 0x1E));
            gfx_rect(tx0 + off, ty0, seg_w, track_h, RGB(0x00, 0xFF, 0x7F));  /* Cachy light green */

            const char *brand = "cachyos";
            int bw = font_text_w(FONT_BOLD, brand, 24 * gg.s);
            font_draw(FONT_BOLD, brand, sw / 2 - bw / 2, ty0 + (int)(26 * gg.s),
                      24 * gg.s, RGB(0x00, 0xFF, 0x7F));
            const char *sub = "Bypassing Hypervisor Security... Core Initialization Active";
            int swd = font_text_w(FONT_UI, sub, 16 * gg.s);
            font_draw(FONT_UI, sub, sw / 2 - swd / 2, ty0 + (int)(64 * gg.s),
                      16 * gg.s, RGB(0x55, 0x55, 0x5A));

            shell_button(910, gg.pl_x, gg.pl_y, gg.pl_w, gg.pl_h,
                         "Load Nobara GUI", RGB(0x1C, 0x1C, 0x1E), RGB(0x00, 0x7F, 0x4F));
            shell_button(918, gg.pl_x, gg.pl_bm_y, gg.pl_w, gg.pl_h,
                         "Boot Manager", RGB(0x1C, 0x1C, 0x1E), RGB(0x00, 0x7F, 0x4F));
        } else {
            /* -- SUB-MODE B: Nobara OS GNOME gaming shell layout -- */
            ShellGeo gg;
            shell_geo(sw, sh, &gg);
            gfx_clear(RGB(0x21, 0x04, 0x1B));   /* Nobara plum background */

            /* top GNOME Shell panel bar */
            gfx_rect(0, 0, sw, gg.nb_panel_h, RGB(0x11, 0x11, 0x13));
            gfx_rect(0, gg.nb_panel_h - 1, sw, 1, RGB(0x25, 0x25, 0x28));

            /* Nobara app-deck toggle (wine-red while the drawer is open) */
            int nb_hov = !S_nobara_menu_open &&
                         ptr_in_rect(g_in.mx, g_in.my, gg.nb_apps_x, gg.nb_apps_y,
                                     gg.nb_apps_w, gg.nb_apps_h);
            gfx_round_rect(gg.nb_apps_x, gg.nb_apps_y, gg.nb_apps_w, gg.nb_apps_h, 6,
                           S_nobara_menu_open ? RGB(0xC1, 0x12, 0x1F)
                                              : (nb_hov ? RGB(0x3A, 0x3A, 0x3E)
                                                        : RGB(0x2D, 0x2D, 0x30)));
            font_draw_string(gg.nb_apps_x + (int)(10 * gg.s),
                             gg.nb_apps_y + (gg.nb_apps_h - 22) / 2 + 1,
                             "Nobara Apps", RGB(0xFF, 0xFF, 0xFF));
            ui_add_hotspot(911, gg.nb_apps_x, gg.nb_apps_y, gg.nb_apps_w, gg.nb_apps_h,
                           "nobara-apps");

            /* panel clock, GNOME style (weekday + 24h) */
            char clk[24];
            panel_clock(clk, sizeof(clk));
            int cw = font_text_w(FONT_UI, clk, 18);
            font_draw(FONT_UI, clk, sw / 2 - cw / 2, (gg.nb_panel_h - 22) / 2, 18,
                      RGB(0xCE, 0xCE, 0xCE));

            /* right cluster: simulated status readouts + splash quick switcher.
             * The readout is right-aligned against the switcher (12px gap) so
             * the cluster can never overlap or clip at any canvas width. */
            const char *stat = "WLAN  VOL  BAT 100%";
            int stw = font_text_w(FONT_UI, stat, 18);
            font_draw(FONT_UI, stat, gg.nb_sw_x - stw - (int)(12 * gg.s),
                      (gg.nb_panel_h - 22) / 2, 18, RGB(0x99, 0x99, 0x9F));
            int sw_hov = ptr_in_rect(g_in.mx, g_in.my, gg.nb_sw_x, gg.nb_apps_y,
                                     gg.nb_sw_w, gg.nb_apps_h);
            gfx_round_rect(gg.nb_sw_x, gg.nb_apps_y, gg.nb_sw_w, gg.nb_apps_h, 6,
                           sw_hov ? RGB(0x3A, 0x3A, 0x3E) : RGB(0x2D, 0x2D, 0x30));
            font_draw_string(gg.nb_sw_x + (int)(10 * gg.s),
                             gg.nb_apps_y + (gg.nb_apps_h - 22) / 2 + 1,
                             "CachyOS Splash", RGB(0xFF, 0xFF, 0xFF));
            ui_add_hotspot(912, gg.nb_sw_x, gg.nb_apps_y, gg.nb_sw_w, gg.nb_apps_h,
                           "cachyos-splash");
        }
        break;

    default:
        gfx_clear(RGB(0, 0, 0));
        break;
    }
}

/* ---------------- modal shell overlays (above the window layer) ----------
 * The cascading Start menu and the Nobara drawer are global modals: they
 * render on top of every open window, exactly like the native OS menus
 * they simulate. Drawn after wm_draw_all() in desktop_frame(). */

/* Single source of truth for Start-menu item collision bands: the renderer,
 * the per-frame hotspots and the pointer router all derive their rectangles
 * from this helper, so hit-test and pixels can never drift apart. */
static void sm_band_rect(const ShellGeo *gg, int i, int *bx, int *by, int *bw, int *bh)
{
    if (!gg || !bx || !by || !bw || !bh) return;   /* defensive */
    *bx = gg->mn_x + (int)(SM_COL_X * gg->s);
    *by = gg->mn_y + (int)((SM_ITEM_OFF[i] - 8) * gg->s);
    *bh = (int)(26 * gg->s);
    /* the power row spans the full lower dashboard panel; app rows are
     * confined to the Column A collision track */
    *bw = (i == 5) ? gg->mn_w - (int)((SM_COL_X + 45) * gg->s)
                   : (int)(SM_COL_W * gg->s);
}

/* Column B "Recent Workspaces" collision bands - same source of truth
 * pattern so the read-only snapshot rows can never drift from their pixels. */
static void ws_band_rect(const ShellGeo *gg, int d, int *bx, int *by, int *bw, int *bh)
{
    if (!gg || !bx || !by || !bw || !bh || d < 0 || d > 3) return;
    *bx = gg->mn_x + (int)(SM_COLB_X * gg->s);
    *by = gg->mn_y + (int)((WS_ITEM_OFF[d] - 8) * gg->s);
    *bw = (int)(190 * gg->s);
    *bh = (int)(26 * gg->s);
}

static void draw_start_menu(void)
{
    ShellGeo gg;
    shell_geo(PSA_W, PSA_H, &gg);
    int mx2 = gg.mn_x, my2 = gg.mn_y, mw2 = gg.mn_w, mh2 = gg.mn_h;

    /* main menu backdrop housing + 1px Fluent border accents (top, right) */
    gfx_rect(mx2, my2, mw2, mh2, RGB(0x1C, 0x1C, 0x1F));
    gfx_rect(mx2, my2, mw2, 1, RGB(0x32, 0x32, 0x36));
    gfx_rect(mx2 + mw2 - 1, my2, 1, mh2, RGB(0x32, 0x32, 0x36));
    /* left branding strip (classic workstation accent) */
    gfx_rect(mx2 + 1, my2 + 1, (int)(28 * gg.s), mh2 - 2, RGB(0x0A, 0x5C, 0xFF));

    /* ---- COLUMN A: pinned system applications & utilities ---- */
    font_draw(FONT_UI, "Pinned Applications", mx2 + (int)(SM_COL_X * gg.s),
              my2 + (int)(20 * gg.s), 18, RGB(0x8E, 0x8E, 0x93));
    gfx_rect(mx2 + (int)(SM_COL_X * gg.s), my2 + (int)(35 * gg.s),
             (int)(SM_COL_W * gg.s), 1, RGB(0x2C, 0x2C, 0x2E));

    static const char *items[6] = {
        "System Specs", "File Explorer", "Command Prompt",
        "Notepad Editor", "Boot Camp Manager", "Disconnect Sandboxed Context Session"
    };
    static const char *hints[6] = { "info", "find", "term", "edit", "", "" };
    static const int ids[6] = { 901, 902, 903, 904, 905, 906 };
    for (int i = 0; i < 6; i++) {
        int bx, by, bw, bh;
        sm_band_rect(&gg, i, &bx, &by, &bw, &bh);
        int sel = (ui_sel_id() == ids[i]);
        int hov = !sel && ptr_in_rect(g_in.mx, g_in.my, bx, by, bw, bh);
        if (sel)
            gfx_rect(bx, by, bw, bh,
                     i == 5 ? RGB(0x7A, 0x1F, 0x1A) : RGB(0x0A, 0x5C, 0xFF));
        else if (hov)   /* subtle pointer lift, distinct from dpad focus */
            gfx_rect(bx, by, bw, bh,
                     i == 5 ? RGB(0x3A, 0x19, 0x17) : RGB(0x26, 0x26, 0x2B));
        font_draw_string(bx + (int)(5 * gg.s), by + (bh - 22) / 2 + 1, items[i],
                         i == 5 ? RGB(0xFF, 0x3B, 0x30) : RGB(0xFF, 0xFF, 0xFF));
        /* right-aligned shortcut hint inside the cell (never overlaps) */
        if (hints[i][0]) {
            int lw = font_text_w(FONT_UI, items[i], 20);
            int hw = font_text_w(FONT_UI, hints[i], 16);
            if (lw + hw + (int)(16 * gg.s) <= bw)
                font_draw(FONT_UI, hints[i], bx + bw - hw - (int)(8 * gg.s),
                          by + (bh - 18) / 2, 16, RGB(0x8E, 0x8E, 0x93));
        }
        ui_add_hotspot(ids[i], bx, by, bw, bh, items[i]);
    }

    /* ---- COLUMN B: recent-workspaces sub-panel (read-only snapshots) ----
     * Every row is live: clicking spawns TextEdit with the simulated file
     * content loaded as a read-only snapshot. Same 26px collision bands. */
    int cbx = mx2 + (int)(SM_COLB_X * gg.s);
    gfx_rect(cbx - (int)(10 * gg.s), my2 + (int)(15 * gg.s), 1,
             mh2 - (int)(60 * gg.s), RGB(0x2C, 0x2C, 0x2E));   /* column separator */
    font_draw(FONT_UI, "Recent Workspaces", cbx, my2 + (int)(20 * gg.s), 18,
              RGB(0x8E, 0x8E, 0x93));
    gfx_rect(cbx, my2 + (int)(35 * gg.s), (int)(190 * gg.s), 1, RGB(0x2C, 0x2C, 0x2E));
    for (int d = 0; d < 4; d++) {
        int bx, by, bw, bh;
        ws_band_rect(&gg, d, &bx, &by, &bw, &bh);
        int sel = (ui_sel_id() == 921 + d);
        int hov = !sel && ptr_in_rect(g_in.mx, g_in.my, bx, by, bw, bh);
        if (sel)
            gfx_rect(bx, by, bw, bh, RGB(0x0A, 0x5C, 0xFF));
        else if (hov)
            gfx_rect(bx, by, bw, bh, RGB(0x26, 0x26, 0x2B));
        font_draw_string(bx + (int)(5 * gg.s), by + (bh - 22) / 2 + 1, WS_DOCS[d],
                         RGB(0xCE, 0xCE, 0xCE));
        ui_add_hotspot(921 + d, bx, by, bw, bh, WS_DOCS[d]);
    }

    /* lower dashboard panel action control: divider + sandbox power row */
    gfx_rect(mx2 + (int)(35 * gg.s), my2 + (int)(275 * gg.s),
             mw2 - (int)(45 * gg.s), 1, RGB(0x2C, 0x2C, 0x2E));
}

static void draw_nobara_drawer(void)
{
    ShellGeo gg;
    shell_geo(PSA_W, PSA_H, &gg);
    int dx = gg.nb_dr_x, dy = gg.nb_dr_y, dw = gg.nb_dr_w, dh = gg.nb_dr_h;

    /* drawer body + wine-red accent edge */
    gfx_rect(dx, dy, dw, dh, RGB(0x18, 0x18, 0x1B));
    gfx_rect(dx, dy, dw, 2, RGB(0xC1, 0x12, 0x1F));

    static const char *items[5] = {
        "Proton GE Runtime Engine", "Gnome Files Browser", "Bash Terminal",
        "Nobara Tweak Utility", "Return to Boot Manager"
    };
    static const int ids[5] = { 913, 914, 915, 916, 917 };
    int band_h = (int)(26 * gg.s);   /* same 26px cell pitch as the Start menu */
    for (int i = 0; i < 5; i++) {
        int by = dy + (int)((NB_ITEM_OFF[i] - 8) * gg.s);
        int sel = (ui_sel_id() == ids[i]);
        int hov = !sel && ptr_in_rect(g_in.mx, g_in.my,
                                      dx + (int)(8 * gg.s), by,
                                      dw - (int)(16 * gg.s), band_h);
        if (sel)
            gfx_rect(dx + (int)(8 * gg.s), by, dw - (int)(16 * gg.s), band_h,
                     RGB(0xC1, 0x12, 0x1F));
        else if (hov)   /* subtle pointer lift on drawer rows */
            gfx_rect(dx + (int)(8 * gg.s), by, dw - (int)(16 * gg.s), band_h,
                     RGB(0x2A, 0x12, 0x16));
        font_draw_string(dx + (int)(20 * gg.s), by + (band_h - 22) / 2 + 1, items[i],
                         RGB(0xFF, 0xFF, 0xFF));
        ui_add_hotspot(ids[i], dx + (int)(8 * gg.s), by,
                       dw - (int)(16 * gg.s), band_h, items[i]);
    }
    /* separator + sandbox status readouts (simulated, informational) */
    gfx_rect(dx + (int)(15 * gg.s), dy + (int)(240 * gg.s),
             dw - (int)(30 * gg.s), 1, RGB(0x2C, 0x2C, 0x30));
    font_draw_string(dx + (int)(20 * gg.s), dy + (int)(260 * gg.s),
                     "System Kernel: 6.x-fsync", RGB(0x00, 0xFF, 0x00));
    font_draw_string(dx + (int)(20 * gg.s), dy + (int)(285 * gg.s),
                     "Execution Sandbox: Secure", RGB(0x00, 0xFF, 0x00));
}

/* ---------------- sandbox shutdown confirm ----------------
 * "Disconnect Sandbox" never exits the payload without an explicit
 * confirmation - the same graceful path as the power dialog. */
static void draw_disconnect_confirm(void)
{
    int w = 560, h = 240;
    int x = (PSA_W - w) / 2, y = (PSA_H - h) / 2 - 60;
    gfx_rect(0, 0, PSA_W, PSA_H, RGBA(0, 0, 0, 120));
    gfx_round_rect(x, y, w, h, 16, RGB(40, 40, 46));
    gfx_round_rect_outline(x, y, w, h, 16, 1, g_theme.window_border);
    font_draw(FONT_BOLD, "Disconnect sandbox?", x + 28, y + 26, 24, g_theme.text_bright);
    font_draw(FONT_UI, "The simulated environment will close and all in-memory", x + 28, y + 64, 19, g_theme.text_dim);
    font_draw(FONT_UI, "state returns to the PS5 home screen. Nothing outside", x + 28, y + 88, 19, g_theme.text_dim);
    font_draw(FONT_UI, "this sandbox is ever touched.", x + 28, y + 112, 19, g_theme.text_dim);
    ui_button(919, x + w - 2 * 200 - 28, y + h - 76, 200, 48, "Cancel", 0);
    ui_button(920, x + w - 200 - 28, y + h - 76, 200, 48, "Disconnect", 1);
    if (ui_pressed(919)) S_disconnect_confirm = 0;
    if (ui_pressed(920)) { S_disconnect_confirm = 0; desktop_begin_shutdown(); }
}

/* ---------------- wallpaper (precomputed) ---------------- */
static void build_wallpaper(int variant)
{
    if (S_wall && S_wall_built == variant) return;
    if (!S_wall) S_wall = (uint32_t *)malloc(sizeof(uint32_t) * PSA_W * PSA_H);
    if (!S_wall) return;
    S_wall_built = variant;

    uint32_t top, bot;
    struct Blob { float x, y, r; uint32_t c; float s; } blobs[7];
    int nb = 0;
    if (variant == 0) {          /* Sonoma Aurora */
        top = RGB(13, 15, 26); bot = RGB(30, 36, 60);
        blobs[nb++] = (struct Blob){ 380, 260, 560, RGBA(20, 160, 170, 255), 0.55f };
        blobs[nb++] = (struct Blob){ 1500, 380, 640, RGBA(30, 90, 220, 255), 0.60f };
        blobs[nb++] = (struct Blob){ 980, 900, 720, RGBA(120, 40, 190, 255), 0.50f };
        blobs[nb++] = (struct Blob){ 1700, 150, 420, RGBA(220, 60, 160, 255), 0.35f };
        blobs[nb++] = (struct Blob){ 600, 620, 480, RGBA(40, 70, 200, 255), 0.35f };
    } else if (variant == 1) {   /* Deep Ocean */
        top = RGB(6, 10, 22); bot = RGB(14, 30, 58);
        blobs[nb++] = (struct Blob){ 500, 300, 620, RGBA(10, 90, 160, 255), 0.55f };
        blobs[nb++] = (struct Blob){ 1400, 700, 700, RGBA(20, 60, 140, 255), 0.55f };
        blobs[nb++] = (struct Blob){ 950, 200, 460, RGBA(30, 160, 200, 255), 0.35f };
        blobs[nb++] = (struct Blob){ 300, 900, 520, RGBA(15, 45, 110, 255), 0.40f };
    } else {                     /* Sunset Ridge */
        top = RGB(28, 16, 30); bot = RGB(60, 26, 34);
        blobs[nb++] = (struct Blob){ 420, 800, 620, RGBA(230, 90, 50, 255), 0.50f };
        blobs[nb++] = (struct Blob){ 1350, 300, 640, RGBA(240, 120, 60, 255), 0.45f };
        blobs[nb++] = (struct Blob){ 950, 950, 560, RGBA(190, 40, 90, 255), 0.45f };
        blobs[nb++] = (struct Blob){ 1650, 850, 480, RGBA(255, 160, 70, 255), 0.35f };
    }
    for (int y = 0; y < PSA_H; y++) {
        uint32_t base = lerp_color(top, bot, (float)y / PSA_H);
        uint32_t *row = &S_wall[(size_t)y * PSA_W];
        for (int x = 0; x < PSA_W; x++) row[x] = base;
    }
    /* render aurora blobs into the cached wallpaper buffer (not the live fb) */
    {
        uint32_t *live = gfx_fb();
        gfx_init(S_wall, PSA_W, PSA_H);
        for (int i = 0; i < nb; i++)
            gfx_glow(blobs[i].x, blobs[i].y, blobs[i].r, blobs[i].c, blobs[i].s);
        gfx_init(live, PSA_W, PSA_H);
    }
}

void desktop_draw_wallpaper(void)
{
    /* Boot Camp theme sync: non-mac targets bypass the Sonoma wallpaper
     * entirely - the OS router owns the full canvas for this frame. */
    if (S_os_target != OS_TARGET_MACOS) {
        desktop_render_background(S_os_target, PSA_W, PSA_H);
        return;
    }
    build_wallpaper(g_settings.wallpaper);
    if (S_wall) {
        uint32_t *fb = gfx_fb();
        memcpy(fb, S_wall, sizeof(uint32_t) * PSA_W * PSA_H);
    } else {
        gfx_vgrad(0, 0, PSA_W, PSA_H, g_theme.wall_top, g_theme.wall_bot);
    }
}

/* ---------------- menu bar ---------------- */
#define MB_H 44

static void draw_clock(char *buf, int n)
{
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    if (g_settings.seconds) {
        const char *am = tmv.tm_hour < 12 ? "AM" : "PM";
        int h = g_settings.clock24 ? tmv.tm_hour : (tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12);
        if (g_settings.clock24)
            psa_snprintf(buf, (size_t)n, "%02d:%02d:%02d", h, tmv.tm_min, tmv.tm_sec);
        else
            psa_snprintf(buf, (size_t)n, "%d:%02d:%02d %s", h, tmv.tm_min, tmv.tm_sec, am);
    }
    else {
        const char *am = tmv.tm_hour < 12 ? "AM" : "PM";
        int h = g_settings.clock24 ? tmv.tm_hour : (tmv.tm_hour % 12 ? tmv.tm_hour % 12 : 12);
        if (g_settings.clock24)
            psa_snprintf(buf, (size_t)n, "%02d:%02d", h, tmv.tm_min);
        else
            psa_snprintf(buf, (size_t)n, "%d:%02d %s", h, tmv.tm_min, am);
    }
}

static const char *focused_app_name(void)
{
    Win *w = wm_top();
    if (!w) return "Finder";
    for (int i = 0; i < APP_COUNT + 1; i++)
        if (g_dock[i].name && !strncmp(w->title, g_dock[i].name, strlen(g_dock[i].name)))
            return g_dock[i].name;
    return w->title;
}

static void draw_menubar(void)
{
    gfx_rect(0, 0, PSA_W, MB_H, g_theme.menubar_bg);

    /* apple menu */
    int sel = (ui_sel_id() == 1);
    icons_draw_apple(28, MB_H / 2, 24.f, sel ? RGB(255, 255, 255) : g_theme.menubar_text);
    ui_add_hotspot(1, 10, 0, 40, MB_H, "apple-menu");

    font_draw(FONT_BOLD, focused_app_name(), 66, 12, 19, g_theme.menubar_text);

    /* right cluster */
    char clk[16];
    draw_clock(clk, sizeof(clk));
    int cw = font_text_w(FONT_UI, clk, 19);
    font_draw(FONT_UI, clk, PSA_W - cw - 24, 12, 19, g_theme.menubar_text);
    int rx = PSA_W - cw - 70;
    icons_draw_glyph(ICON_CC, rx, MB_H / 2, 22, g_theme.menubar_text);
    ui_add_hotspot(4, rx - 14, 0, 28, MB_H, "control-center");
    rx -= 44;
    icons_draw_glyph(ICON_BATTERY, rx, MB_H / 2, 26, g_theme.menubar_text);
    ui_add_hotspot(3, rx - 18, 0, 36, MB_H, "battery");
    rx -= 46;
    icons_draw_glyph(ICON_WIFI, rx, MB_H / 2 - 4, 24, g_theme.menubar_text);
    ui_add_hotspot(2, rx - 16, 0, 32, MB_H, "wifi");

    /* apple dropdown */
    if (S_apple_open) {
        int mx = 16, my = MB_H, mw = 300;
        struct { const char *label; int id; int sep_after; } items[] = {
            { "About This Mac",   10, 1 },
            { "System Settings",  11, 1 },
            { "Sleep",            12, 1 },
            { "Restart...",       13, 0 },
        };
        int ih = 44, hh = 8;
        int total = hh * 2 + ih * 4;
        gfx_round_rect(mx, my, mw, total, 12, RGBA(44, 44, 48, 248));
        int yy = my + hh;
        for (int i = 0; i < 4; i++) {
            int sel2 = (ui_sel_id() == items[i].id);
            if (sel2) gfx_round_rect(mx + 6, yy, mw - 12, ih, 8, g_theme.accent);
            font_draw(FONT_UI, items[i].label, mx + 20, yy + 10, 19,
                      sel2 ? RGB(255, 255, 255) : g_theme.text);
            ui_add_hotspot(items[i].id, mx + 6, yy, mw - 12, ih, items[i].label);
            yy += ih;
            if (items[i].sep_after) gfx_hline(mx + 12, mx + mw - 12, yy, g_theme.separator), yy += 4;
        }
    }
}

/* ---------------- dock ---------------- */
#define DOCK_ICON 84
#define DOCK_PAD  18

static void draw_dock(void)
{
    int n = APP_COUNT + 1;
    int dw = n * DOCK_ICON + (n - 1) * DOCK_PAD + 2 * DOCK_PAD;
    int dx = (PSA_W - dw) / 2;
    int dy = PSA_H - DOCK_ICON - 34;

    gfx_round_rect(dx, dy, dw, DOCK_ICON + 2 * DOCK_PAD, 26, g_theme.dock_bg);
    gfx_round_rect_outline(dx, dy, dw, DOCK_ICON + 2 * DOCK_PAD, 26, 1, g_theme.dock_border);

    for (int i = 0; i < n; i++) {
        int id = 100 + i;
        int ix = dx + DOCK_PAD + i * (DOCK_ICON + DOCK_PAD);
        int iy = dy + DOCK_PAD;
        int sel = (ui_sel_id() == id);
        int running = (g_dock[i].app != APP_TRASH) && wm_find(g_dock[i].app + 1);

        /* hover raise */
        int oy = sel ? -10 : 0;
        if (sel) {
            gfx_round_rect(ix - 8, iy - 14, DOCK_ICON + 16, DOCK_ICON + 22, 16, RGBA(255, 255, 255, 36));
            /* tooltip */
            int lw = font_text_w(FONT_UI, g_dock[i].name, 18);
            int txx = ix + DOCK_ICON / 2 - lw / 2;
            gfx_round_rect(txx - 12, iy - 52, lw + 24, 32, 9, RGBA(60, 60, 66, 235));
            font_draw(FONT_UI, g_dock[i].name, txx, iy - 46, 18, RGB(255, 255, 255));
        }
        icons_draw_app_icon(g_dock[i].icon, ix, iy + oy, DOCK_ICON, 0);
        if (running) gfx_circle_fill(ix + DOCK_ICON / 2, dy + DOCK_ICON + 2 * DOCK_PAD - 6, 4, RGB(220, 220, 230));
        ui_add_hotspot(id, ix, iy, DOCK_ICON, DOCK_ICON, g_dock[i].name);
    }
}

/* ---------------- cursor + selection ring ---------------- */
static void draw_arrow(int x, int y)
{
    static const float ax[] = { 0, 0, 4.5f, 8, 10.5f, 6.5f, 12 };
    static const float ay[] = { 0, 16.5f, 12.5f, 9.5f, 18.5f, 10.5f, 10 };
    float tx[7], ty[7];
    for (int i = 0; i < 7; i++) { tx[i] = x + ax[i]; ty[i] = y + ay[i]; }
    gfx_poly_fill(tx, ty, 7, RGB(255, 255, 255));
    for (int i = 0; i < 7; i++) {
        int j = (i + 1) % 7;
        gfx_line((int)tx[i], (int)ty[i], (int)tx[j], (int)ty[j], RGB(0, 0, 0));
    }
}

static void draw_selection_ring(void)
{
    int x, y, w, h;
    ui_sel_rect(&x, &y, &w, &h);
    if (w <= 0) return;
    gfx_round_rect_outline(x, y, w, h, 14, 3, g_theme.accent);
    /* corner glows */
    draw_arrow(x + w - 6, y + h - 2);
}

/* ---------------- power dialog ---------------- */
static void draw_power_dialog(void)
{
    int w = 560, h = 240;
    int x = (PSA_W - w) / 2, y = (PSA_H - h) / 2 - 60;
    gfx_rect(0, 0, PSA_W, PSA_H, RGBA(0, 0, 0, 120));
    gfx_round_rect(x, y, w, h, 16, RGB(40, 40, 46));
    gfx_round_rect_outline(x, y, w, h, 16, 1, g_theme.window_border);
    font_draw(FONT_BOLD, "Restart PS5GreenApple?", x + 28, y + 26, 24, g_theme.text_bright);
    font_draw(FONT_UI, "The desktop environment will close and control", x + 28, y + 64, 19, g_theme.text_dim);
    font_draw(FONT_UI, "returns to the PS5 home screen.", x + 28, y + 88, 19, g_theme.text_dim);
    int bw = 200;
    if (ui_button(20, x + w - 2 * bw - 28, y + h - 76, bw, 48, "Cancel", 0)) { /* handled below */ }
    if (ui_button(21, x + w - bw - 28, y + h - 76, bw, 48, "Restart", 1)) { /* handled below */ }
    if (ui_pressed(20)) S_power_open = 0;
    if (ui_pressed(21)) { S_power_open = 0; desktop_begin_shutdown(); }
}

/* ---------------- main frame ---------------- */
void desktop_init(void)
{
    font_init();
    S_state = DS_BOOT;
    S_apple_open = 0;
    S_power_open = 0;
    S_toast_ms = 0;
    S_wall = NULL;
    S_wall_built = -1;
    S_start_menu_open = 0;
    S_nobara_menu_open = 0;
    S_linux_flavor = 1;
    S_plymouth_ticks = 0;
    S_last_target = -1;
    S_disconnect_confirm = 0;
    sysinfo_mark_boot();
    boot_reset();
    wm_init();
    theme_init();
    S_last_activity = now_ms();
}

static void handle_global_keys(void)
{
    /* options -> power dialog */
    if (in_clicked(BTN_OPTIONS) && !osk_active()) {
        S_power_open ^= 1;
        return;
    }
    if (in_clicked(BTN_CIRCLE)) {
        if (S_apple_open) { S_apple_open = 0; return; }
        if (S_disconnect_confirm) { S_disconnect_confirm = 0; return; }
        if (S_start_menu_open) { S_start_menu_open = 0; return; }
        if (S_nobara_menu_open) { S_nobara_menu_open = 0; return; }
        if (osk_active()) return;         /* osk handles it */
        wm_close_top();
    }
    if (in_clicked(BTN_SQUARE)) {
        Win *t = wm_top();
        if (t && t->id == WIN_TEXTEDIT) app_textedit_save();
    }
    if (in_clicked(BTN_R1) || in_clicked(BTN_R2)) {
        Win *t = wm_top();
        if (t) t->scroll_off += 44;
    }
    if (in_clicked(BTN_L1) || in_clicked(BTN_L2)) {
        Win *t = wm_top();
        if (t && t->scroll_off > 0) t->scroll_off -= 44;
        if (t && t->scroll_off < 0) t->scroll_off = 0;
    }
}

static void dispatch_action(int id)
{
    if (id >= 60 && id < 80) {           /* window close buttons */
        wm_close(id - 60);
        return;
    }
    if (id >= 100 && id < 100 + APP_COUNT + 1) {
        desktop_open_app(g_dock[id - 100].app);
        return;
    }
    switch (id) {
    case 1: S_apple_open ^= 1; break;
    case 10: S_apple_open = 0; app_about_open(); break;
    case 11: S_apple_open = 0; app_settings_open(); break;
    case 12: S_apple_open = 0; S_state = DS_SLEEP; break;
    case 13: S_apple_open = 0; desktop_begin_shutdown(); break;
    /* ---- interactive shell chrome: Windows Start menu modal ---- */
    case 107:                                /* legacy taskbar-start id */
    case 900: desktop_toggle_start_menu(); break;
    case 901: S_start_menu_open = 0; app_about_open(); break;     /* System Specs */
    case 902: S_start_menu_open = 0; app_finder_open(); break;    /* File Explorer */
    case 903: S_start_menu_open = 0; app_terminal_open(); break;  /* Command Prompt */
    case 904: S_start_menu_open = 0; app_textedit_open(); break;  /* Notepad Editor */
    case 905: S_start_menu_open = 0; app_bootcamp_open(); break;  /* Boot Camp Manager */
    case 906: S_start_menu_open = 0; S_disconnect_confirm = 1; break;  /* Disconnect */
    case 907: app_bootcamp_open(); break;     /* taskbar Boot Camp tray */
    /* Start menu Column B: read-only workspace snapshots in TextEdit */
    case 921: case 922: case 923: case 924: {
        int d = id - 921;
        if (d >= 0 && d < 4) {                /* defensive id clamp */
            S_start_menu_open = 0;
            app_textedit_open_content(WS_DOCS[d], WS_TEXT[d]);
        }
        break;
    }
    /* ---- interactive shell chrome: Linux dual-flavor loop ---- */
    case 910: desktop_set_linux_flavor(0); desktop_toast("Nobara GNOME shell online"); break;
    case 911: desktop_toggle_nobara_menu(); break;
    case 912: desktop_set_linux_flavor(1); desktop_toast("Returning to CachyOS splash"); break;
    case 913: S_nobara_menu_open = 0; desktop_toast("Proton GE runtime: simulated launch"); break;
    case 914: S_nobara_menu_open = 0; app_finder_open(); break;
    case 915: S_nobara_menu_open = 0; app_terminal_open(); break;
    case 916: S_nobara_menu_open = 0; app_settings_open(); break;
    case 917: S_nobara_menu_open = 0; app_bootcamp_open(); break;
    case 918: app_bootcamp_open(); break;     /* Plymouth Boot Manager */
    case 919: S_disconnect_confirm = 0; break;
    case 920: S_disconnect_confirm = 0; desktop_begin_shutdown(); break;
    default: break;                       /* app-internal ids handled by apps */
    }
}

void desktop_frame(void)
{
    ui_begin_frame();

    if (S_state == DS_SHUTDOWN) {
        static uint64_t t0 = 0;
        if (!t0) t0 = now_ms();
        desktop_draw_wallpaper();
        float a = clampf((float)(now_ms() - t0) / 800.f, 0.f, 1.f);
        gfx_rect(0, 0, PSA_W, PSA_H, RGBA(0, 0, 0, (int)(255 * a)));
        if (a >= 1.f) { g_quit_request = 1; t0 = 0; }
        return;
    }
    if (S_state == DS_SLEEP) {
        gfx_clear(RGB(0, 0, 0));
        if (g_in.clicked) S_state = DS_RUN;
        return;
    }
    if (boot_active()) {
        int done = boot_frame();
        if (done) {
            S_state = DS_RUN;
            S_need_initial_sel = 1;    /* apply after first desktop draw */
        }
        return;
    }

    /* ---- active desktop ---- */
    if (g_in.clicked || g_in.buttons) S_last_activity = now_ms();

    /* Foreign shells are first-class persistent environments now: the
     * Windows taskbar carries a working Start menu and the Linux loop
     * always keeps a Boot Manager escape hatch on screen, so every shell
     * can reach the Boot Camp manager (and from it the macOS shell) in a
     * single click - no automatic revert needed. */

    handle_global_keys();

    /* dpad navigation */
    if (in_clicked(BTN_UP))    ui_nav(0);
    if (in_clicked(BTN_DOWN))  ui_nav(1);
    if (in_clicked(BTN_LEFT))  ui_nav(2);
    if (in_clicked(BTN_RIGHT)) ui_nav(3);

    if (in_clicked(BTN_CROSS) && !osk_active()) dispatch_action(ui_sel_id());

    /* ---- draw ----
     * In Boot Camp foreign-shell mode the macOS menubar/dock are replaced
     * by the target OS chrome rendered inside the background router. */
    desktop_draw_wallpaper();
    if (S_os_target == OS_TARGET_MACOS) draw_menubar();
    wm_draw_all();
    if (S_os_target == OS_TARGET_MACOS) draw_dock();

    /* modal shell overlays float above the window layer (native z-order) */
    if (S_os_target == OS_TARGET_WINDOWS && S_start_menu_open) draw_start_menu();
    if (S_os_target == OS_TARGET_LINUX && desktop_get_linux_flavor() == 0 &&
        S_nobara_menu_open) draw_nobara_drawer();

    if (S_disconnect_confirm) draw_disconnect_confirm();
    if (S_power_open) draw_power_dialog();
    if (osk_active()) osk_frame();

    /* toast */
    if (S_toast_ms && now_ms() - (uint64_t)S_toast_ms < 2600) {
        int tw = font_text_w(FONT_UI, S_toast, 20);
        int tx = PSA_W - tw - 72, ty = MB_H + 18;
        gfx_round_rect(tx - 20, ty - 10, tw + 40, 44, 10, RGBA(50, 50, 56, 235));
        gfx_round_rect_outline(tx - 20, ty - 10, tw + 40, 44, 10, 1, g_theme.window_border);
        font_draw(FONT_UI, S_toast, tx, ty, 20, g_theme.text);
    }

    if (g_settings.fps) {
        static int frames = 0; static uint64_t last = 0;
        frames++;
        char buf[32] = "";   /* zero-initialized: defined read before first tick */
        if (now_ms() - last > 1000) {
            psa_snprintf(buf, sizeof(buf), "%d fps", frames);
            last = now_ms(); frames = 0;
        }
        font_draw(FONT_MONO, buf[0] ? buf : "...", 20, MB_H + 12, 16, RGB(120, 255, 140));
    }

    draw_selection_ring();

    /* pointer click routing: DualSense touchpad tap (PS5) or host mouse.
     * Runs after the draw pass so this frame's hotspots are live for the
     * generic fallback path. Shell chrome is hit-tested first. */
    if (in_clicked(BTN_TOUCHPAD) && !osk_active())
        desktop_handle_click(g_in.mx, g_in.my);

    if (S_need_initial_sel) {
        ui_set_sel(100);               /* focus Finder in the dock */
        S_need_initial_sel = 0;
    }
}

/* ---------------- pointer click router (touchpad tap / host mouse) -------
 * Coordinate hit-test against the interactive shell chrome. Mirrors the
 * geometry tables used by the renderer through the shared shell_geo()
 * source of truth, so render and hit-test can never drift apart.
 * Returns 1 when the click was consumed. */
int desktop_handle_click(int mx, int my)
{
    if (mx < 0 || my < 0 || mx >= PSA_W || my >= PSA_H) return 0;   /* defensive */
    ShellGeo gg;
    shell_geo(PSA_W, PSA_H, &gg);

    if (S_os_target == OS_TARGET_WINDOWS) {
        /* 1. Start button hit -> toggle the cascading modal */
        if (mx >= gg.st_x && mx <= gg.st_x + gg.st_w &&
            my >= gg.st_y && my <= gg.st_y + gg.st_h) {
            desktop_toggle_start_menu();
            return 1;
        }
        /* 2. taskbar Boot Camp tray (permanent escape hatch) */
        if (mx >= gg.bc_x && mx <= gg.bc_x + gg.bc_w &&
            my >= gg.bc_y && my <= gg.bc_y + gg.bc_h) {
            app_bootcamp_open();
            return 1;
        }
        /* 3. cascading menu items -> safe app dispatch. Multi-column input
         *    collision: Column A app rows respond inside their 180px (720p)
         *    collision track via the shared sm_band_rect() source of truth;
         *    the power row spans the full lower dashboard panel; Column B
         *    rows open read-only workspace snapshots. Clicks landing in the
         *    menu body are absorbed without closing - zero render overhead. */
        if (S_start_menu_open &&
            mx >= gg.mn_x && mx <= gg.mn_x + gg.mn_w &&
            my >= gg.mn_y && my <= gg.mn_y + gg.mn_h) {
            static const int ids[6] = { 901, 902, 903, 904, 905, 906 };
            for (int i = 0; i < 6; i++) {
                int bx, by, bw, bh;
                sm_band_rect(&gg, i, &bx, &by, &bw, &bh);
                if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                    ui_set_sel(ids[i]);
                    dispatch_action(ids[i]);
                    return 1;
                }
            }
            /* Column B: recent-workspace rows -> read-only TextEdit views */
            for (int d = 0; d < 4; d++) {
                int bx, by, bw, bh;
                ws_band_rect(&gg, d, &bx, &by, &bw, &bh);
                if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                    ui_set_sel(921 + d);
                    dispatch_action(921 + d);
                    return 1;
                }
            }
            return 1;   /* click inside the menu body: absorbed, menu stays */
        }
        /* 4. click outside menu + taskbar: native modal dismissal */
        if (S_start_menu_open) {
            S_start_menu_open = 0;
            return 1;
        }
    } else if (S_os_target == OS_TARGET_LINUX) {
        if (desktop_get_linux_flavor() == 1) {
            /* Plymouth splash: enter the Nobara shell / reach Boot Manager */
            if (mx >= gg.pl_x && mx <= gg.pl_x + gg.pl_w) {
                if (my >= gg.pl_y && my <= gg.pl_y + gg.pl_h) {
                    desktop_set_linux_flavor(0);
                    desktop_toast("Nobara GNOME shell online");
                    return 1;
                }
                if (my >= gg.pl_bm_y && my <= gg.pl_bm_y + gg.pl_h) {
                    app_bootcamp_open();
                    return 1;
                }
            }
        } else {
            /* Nobara panel: app-deck toggle + splash quick switcher */
            if (my >= gg.nb_apps_y && my <= gg.nb_apps_y + gg.nb_apps_h) {
                if (mx >= gg.nb_apps_x && mx <= gg.nb_apps_x + gg.nb_apps_w) {
                    desktop_toggle_nobara_menu();
                    return 1;
                }
                if (mx >= gg.nb_sw_x && mx <= gg.nb_sw_x + gg.nb_sw_w) {
                    desktop_set_linux_flavor(1);
                    desktop_toast("Returning to CachyOS splash");
                    return 1;
                }
            }
            /* drawer items -> safe app dispatch */
            if (S_nobara_menu_open &&
                mx >= gg.nb_dr_x && mx <= gg.nb_dr_x + gg.nb_dr_w &&
                my >= gg.nb_dr_y && my <= gg.nb_dr_y + gg.nb_dr_h) {
                static const int ids[5] = { 913, 914, 915, 916, 917 };
                int band_h = (int)(34 * gg.s);
                for (int i = 0; i < 5; i++) {
                    int by = gg.nb_dr_y + (int)((NB_ITEM_OFF[i] - 8) * gg.s);
                    if (my >= by && my < by + band_h) {
                        ui_set_sel(ids[i]);
                        dispatch_action(ids[i]);
                        return 1;
                    }
                }
                return 1;   /* inside the drawer body: absorbed */
            }
            if (S_nobara_menu_open) { S_nobara_menu_open = 0; return 1; }
        }
    }

    /* generic fallback: topmost hotspot under the pointer (dock, close
     * buttons, dialogs, menu bars). Desktop-level ids dispatch immediately;
     * app-internal ids only take focus (their owners poll ui_pressed). */
    for (int i = ui_hotspot_count() - 1; i >= 0; i--) {
        Hotspot *h = ui_hotspot_at(i);
        if (!h) continue;
        if (mx >= h->x && mx < h->x + h->w && my >= h->y && my < h->y + h->h) {
            ui_set_sel(h->id);
            int shell_id = (h->id >= 60 && h->id < 80) ||   /* window close   */
                           (h->id >= 100 && h->id < 110) || /* dock           */
                           (h->id >= 900 && h->id < 1000) ||/* shell chrome   */
                           (h->id >= 1 && h->id <= 30);     /* menubar/dialog */
            if (shell_id) dispatch_action(h->id);
            return 1;
        }
    }
    return 0;
}
