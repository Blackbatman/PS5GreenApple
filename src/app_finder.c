/* app_finder.c - real filesystem browser (Finder) */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "desktop.h"
#include "common.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { char name[128]; char path[512]; int is_dir; long size; } FEntry;
typedef struct {
    char cwd[512];
    FEntry ent[128];
    int n;
    char tip[160];
} Finder;

static Finder S_f;
static void finder_draw(Win *w);   /* defined below, used by open */

static void finder_reload(void)
{
    S_f.n = 0;
    DIR *d = opendir(S_f.cwd);
    if (!d) {
        psa_snprintf(S_f.tip, sizeof(S_f.tip), "Cannot open %s", S_f.cwd);
        return;
    }
    struct dirent *de;
    while ((de = readdir(d)) && S_f.n < 128) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        FEntry *e = &S_f.ent[S_f.n];
        psa_strcpy(e->name, sizeof(e->name), de->d_name);
        path_join(e->path, sizeof(e->path), S_f.cwd, de->d_name);
        struct stat st;
        e->is_dir = 0; e->size = 0;
        if (stat(e->path, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->size = (long)st.st_size;
        }
        S_f.n++;
    }
    closedir(d);
    /* dirs first, alpha */
    for (int i = 0; i < S_f.n; i++)
        for (int j = i + 1; j < S_f.n; j++) {
            int a = S_f.ent[i].is_dir * 2 + S_f.ent[j].is_dir;
            int swap = (a == 1) || (a == 0 && strcmp(S_f.ent[i].name, S_f.ent[j].name) > 0)
                                 || (a == 3 && strcmp(S_f.ent[i].name, S_f.ent[j].name) > 0);
            if (swap) { FEntry t = S_f.ent[i]; S_f.ent[i] = S_f.ent[j]; S_f.ent[j] = t; }
        }
    psa_snprintf(S_f.tip, sizeof(S_f.tip), "%d items", S_f.n);
}

void app_finder_open(void)
{
    if (!wm_find(WIN_FINDER)) {
        if (!S_f.cwd[0]) {
#ifdef __PS5__
            psa_strcpy(S_f.cwd, sizeof(S_f.cwd), "/data");
#else
            psa_strcpy(S_f.cwd, sizeof(S_f.cwd), ".");
#endif
        }
        finder_reload();
        wm_open(WIN_FINDER, "Finder", CHROME_MAC, 340, 120, 1180, 700, finder_draw);
    } else {
        wm_raise(wm_find(WIN_FINDER));
    }
}

#define SIDEBAR_W 240

static void finder_draw(Win *w)
{
    /* sidebar */
    gfx_rect(w->x + 1, w->y + 42, SIDEBAR_W, w->h - 44, g_theme.sidebar_bg);
    font_draw(FONT_BOLD, "Favorites", w->x + 24, w->y + 64, 17, g_theme.text_dim);
    struct { const char *name; const char *path; } favs[] = {
#ifdef __PS5__
        { "Data", "/data" }, { "App0", "/app0" }, { "USB0", "/mnt/usb0" },
        { "User", "/user" }, { "System", "/system_data" }, { "Root", "/" },
#else
        { "Home", "." }, { "Root", "/" }, { "Etc", "/etc" }, { "Usr", "/usr" },
#endif
    };
    int fy = w->y + 96;
    for (int i = 0; i < (int)(sizeof(favs) / sizeof(favs[0])); i++) {
        int id = 200 + i;
        int sel = (ui_sel_id() == id);
        if (sel) gfx_round_rect(w->x + 12, fy - 6, SIDEBAR_W - 24, 40, 8, g_theme.accent);
        icons_draw_glyph(ICON_FOLDER, w->x + 40, fy + 13, 20, sel ? RGB(255, 255, 255) : g_theme.accent);
        font_draw(FONT_UI, favs[i].name, w->x + 62, fy + 3, 18, sel ? RGB(255, 255, 255) : g_theme.text);
        ui_add_hotspot(id, w->x + 12, fy - 6, SIDEBAR_W - 24, 40, favs[i].name);
        if (ui_pressed(id)) {
            psa_strcpy(S_f.cwd, sizeof(S_f.cwd), favs[i].path);
            finder_reload();
            w->scroll_off = 0;
        }
        fy += 46;
    }

    /* path bar */
    int cx = w->x + SIDEBAR_W + 16;
    int cw = w->w - SIDEBAR_W - 32;
    gfx_round_rect(cx, w->y + 56, cw, 36, 8, g_theme.input_bg);
    font_draw(FONT_UI, S_f.cwd, cx + 12, w->y + 63, 17, g_theme.text_dim);
    font_draw(FONT_UI, S_f.tip, cx + cw - font_text_w(FONT_UI, S_f.tip, 15) - 12, w->y + 65, 15, g_theme.text_dim);

    /* file list */
    int list_y = w->y + 108;
    int row_h = 46;
    int visible = (w->y + w->h - 20 - list_y) / row_h;
    int start = w->scroll_off / row_h;
    if (start < 0) start = 0;

    for (int i = 0; (i < visible) && (start + i < S_f.n); i++) {
        FEntry *e = &S_f.ent[start + i];
        int id = 230 + i;
        int ry = list_y + i * row_h;
        int sel = (ui_sel_id() == id);
        if (sel) gfx_round_rect(cx, ry - 4, cw, row_h - 8, 8, g_theme.row_hover);
        icons_draw_glyph(e->is_dir ? ICON_FOLDER : (strstr(e->name, ".png") || strstr(e->name, ".jpg") ? ICON_IMAGE : ICON_FILE),
                         cx + 24, ry + row_h / 2 - 2, 24,
                         e->is_dir ? g_theme.accent : g_theme.text_dim);
        font_draw(FONT_UI, e->name, cx + 48, ry + (row_h - 24) / 2, 18, g_theme.text);
        char sz[32];
        if (e->is_dir) psa_strcpy(sz, sizeof(sz), "--");
        else if (e->size > 1048576) psa_snprintf(sz, sizeof(sz), "%.1f MB", e->size / 1048576.0);
        else if (e->size > 1024) psa_snprintf(sz, sizeof(sz), "%ld KB", e->size / 1024);
        else psa_snprintf(sz, sizeof(sz), "%ld B", e->size);
        font_draw(FONT_UI, sz, cx + cw - font_text_w(FONT_UI, sz, 15) - 16, ry + (row_h - 22) / 2, 15, g_theme.text_dim);
        ui_add_hotspot(id, cx, ry - 4, cw, row_h - 8, e->name);
        if (ui_pressed(id)) {
            if (e->is_dir) {
                psa_strcpy(S_f.cwd, sizeof(S_f.cwd), e->path);
                finder_reload();
                w->scroll_off = 0;
            } else if (strstr(e->name, ".txt")) {
                app_textedit_open_path(e->path);
            }
        }
    }
    if (S_f.n == 0)
        font_draw(FONT_UI, "Empty folder", cx + 24, list_y + 16, 18, g_theme.text_dim);

    /* scroll range clamp */
    int max_off = S_f.n * row_h - (w->h - 128);
    if (max_off < 0) max_off = 0;
    if (w->scroll_off > max_off) w->scroll_off = max_off;
}
