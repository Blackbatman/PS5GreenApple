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

/* Boot Camp OS-shell target: which environment the background canvas is
 * currently simulating. Pure local UI state - switching only re-routes
 * the background renderer, nothing outside the sandbox is touched. */
static int S_os_target = OS_TARGET_MACOS;

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
    S_os_target = os;
}

int desktop_os_target(void) { return S_os_target; }

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

    case OS_TARGET_WINDOWS:
        /* Windows: Classic Midnight Teal Workstation layout (Fluent Core). */
        gfx_clear(RGB(0x00, 0x5A, 0x5B));
        gfx_rect(0, sh - (int)(45 * s), sw, (int)(45 * s), RGB(0x10, 0x10, 0x10)); /* taskbar */
        gfx_rect((int)(5 * s), sh - (int)(40 * s), (int)(60 * s), (int)(35 * s),
                 RGB(0x1F, 0x1F, 0x1F));                             /* start button */
        font_draw_string(18, sh - (int)(28 * s), "Start", RGB(0x00, 0xFF, 0x00));
        /* pinned Boot Camp launcher on the taskbar = permanent way back
         * into the boot manager while a non-mac shell is active */
        ui_add_hotspot(107, (int)(5 * s), sh - (int)(45 * s),
                       (int)(60 * s) + 24, (int)(45 * s), "taskbar-start");
        break;

    case OS_TARGET_LINUX:
        /* Linux: Wayland-inspired Dark Auburn core (GNOME shell). */
        gfx_clear(RGB(0x2D, 0x09, 0x22));
        gfx_rect(0, 0, sw, (int)(30 * s), RGB(0x11, 0x11, 0x11));   /* top panel */
        font_draw_string(sw / 2 - 30, 8, "Activities", RGB(0xCE, 0xCE, 0xCE));
        break;

    default:
        gfx_clear(RGB(0, 0, 0));
        break;
    }
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
        if (S_power_open) { S_power_open = 0; return; }
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
    case 107: desktop_open_app(APP_BOOTCAMP); break;   /* taskbar start (Windows shell) */
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

    /* Boot Camp guard: if the user closed every window while a foreign
     * shell was simulated, fall back to the macOS environment so the
     * dock (and every launch path) is always reachable again. */
    if (S_os_target != OS_TARGET_MACOS && !wm_any_open()) {
        S_os_target = OS_TARGET_MACOS;
        desktop_toast("Returning to macOS Aqua Slate shell");
    }

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

    if (S_need_initial_sel) {
        ui_set_sel(100);               /* focus Finder in the dock */
        S_need_initial_sel = 0;
    }
}
