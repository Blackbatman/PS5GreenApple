/* app_textedit.c - TextEdit with OSK editing + save to PSA_DATA_DIR */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "osk.h"
#include "desktop.h"
#include "common.h"
#include <sys/stat.h>

#define TE_MAX 4096
#define TE_PATH_MAX 512

typedef struct {
    char text[TE_MAX];
    char path[TE_PATH_MAX];
    int  dirty;
    int  editing;            /* OSK active */
} TextEdit;

static TextEdit S_te;
static void te_draw(Win *w);   /* forward decl */

static void te_load(const char *path)
{
    memset(&S_te, 0, sizeof(S_te));
    if (path) {
        psa_strcpy(S_te.path, sizeof(S_te.path), path);
        FILE *f = fopen(path, "r");
        if (f) {
            size_t n = fread(S_te.text, 1, TE_MAX - 1, f);
            S_te.text[n] = 0;
            fclose(f);
        }
    } else {
        psa_strcpy(S_te.path, sizeof(S_te.path), PSA_DATA_DIR "/notes.txt");
        psa_strcpy(S_te.text, sizeof(S_te.text),
                   "Welcome to TextEdit!\n\n"
                   "This buffer lives on the PS5 filesystem.\n\n"
                   "Controls:\n"
                   "  X      - open the on-screen keyboard\n"
                   "  SQUARE - save to " PSA_DATA_DIR "/notes.txt\n"
                   "  O      - close the keyboard\n\n"
                   "Start typing to replace this text.\n");
    }
}

void app_textedit_open_path(const char *path) { te_load(path); app_textedit_open(); }

void app_textedit_save(void)
{
    ensure_dir(PSA_DATA_DIR);
    FILE *f = fopen(S_te.path, "w");
    if (!f) {
        desktop_toast("Save failed: %s", S_te.path);
        return;
    }
    fwrite(S_te.text, 1, strlen(S_te.text), f);
    fclose(f);
    S_te.dirty = 0;
    desktop_toast("Saved to %s", S_te.path);
}

void app_textedit_open(void)
{
    if (!wm_find(WIN_TEXTEDIT)) {
        if (!S_te.path[0]) te_load(NULL);
        wm_open(WIN_TEXTEDIT, "TextEdit", CHROME_MAC, 420, 140, 1080, 680, te_draw);
    } else {
        wm_raise(wm_find(WIN_TEXTEDIT));
    }
}

static void te_draw(Win *w)
{
    int pad = 26;
    int x = w->x + pad, y = w->y + 60;
    int cw = w->w - 2 * pad;
    int ch = w->h - 60 - 120;

    /* paper */
    gfx_round_rect(x, y, cw, ch, 12, RGB(38, 38, 42));
    gfx_push_clip(x + 1, y + 1, cw - 2, ch - 2);
    int line_h = 26;
    int start = w->scroll_off / line_h;
    if (start < 0) start = 0;
    int max_lines = (ch - 20) / line_h;

    /* split text into lines */
    const char *p = S_te.text;
    int skip = start;
    while (*p && skip > 0) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
        skip--;
    }
    int yy = y + 12;
    int chars_shown = 0;
    for (int i = 0; i < max_lines && *p; i++) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 96) len = 96;
        char linebuf[128];
        memcpy(linebuf, p, (size_t)len); linebuf[len] = 0;
        font_draw(FONT_MONO, linebuf, x + 14, yy, 17, RGB(225, 228, 235));
        yy += line_h;
        chars_shown += len + 1;
        if (!nl) break;
        p = nl + 1;
    }
    gfx_pop_clip();

    /* scrollbar hint */
    int total_lines = 1;
    for (const char *q = S_te.text; *q; q++) if (*q == '\n') total_lines++;
    int max_off = total_lines * line_h - (ch - 24);
    if (max_off < 0) max_off = 0;
    if (w->scroll_off > max_off) w->scroll_off = max_off;

    /* toolbar */
    int ty = y + ch + 16;
    int bx = x;
    if (ui_button(400, bx, ty, 190, 46, S_te.editing ? "Close keys" : "Edit (X)", 0)) { }
    bx += 200;
    if (ui_button(401, bx, ty, 170, 46, "Save (SQR)", 1)) { }
    if (ui_pressed(400)) {
        if (S_te.editing) { osk_close(); S_te.editing = 0; }
        else { osk_open(S_te.text, TE_MAX, "TextEdit", OSK_FLAG_NONE); S_te.editing = 1; }
    }
    if (ui_pressed(401)) app_textedit_save();

    /* status */
    char st[160];
    psa_snprintf(st, sizeof(st), "%s%s  |  %d bytes",
                 S_te.editing ? "editing  |  " : "",
                 S_te.path, (int)strlen(S_te.text));
    font_draw(FONT_UI, st, x + 396, ty + 13, 16, g_theme.text_dim);

    /* editing via OSK: buffer shared */
    if (S_te.editing && !osk_active()) S_te.editing = 0;  /* user closed keys */
    if (S_te.dirty) S_te.dirty = 0;
}
