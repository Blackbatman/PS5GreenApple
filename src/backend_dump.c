/* backend_dump.c - headless test backend (host)
 *
 * Renders into an in-memory framebuffer and executes a screenshot script:
 *   wait <ms>      advance virtual time (20x real time)
 *   press <btn>    press a button (cross|circle|square|triangle|up|down|...)
 *   move <x> <y>   move pointer
 *   type <text>    inject characters into the on-screen keyboard
 *   shot <name>    write outdir/<name>.png
 *   exit           finish
 *
 * Used by `make shots` to produce the documentation screenshots.
 */
#include "common.h"
#include "gfx.h"
#include "osk.h"
#include "ui.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <time.h>

uint64_t g_vclock = 0;

enum { CMD_WAIT, CMD_PRESS, CMD_MOVE, CMD_SHOT, CMD_EXIT, CMD_TYPE, CMD_SEL };

typedef struct {
    int type;
    int a, b;
    char s[64];
} Cmd;

static Cmd S_cmds[128];
static int S_ncmd = 0, S_cur = 0;
static uint64_t S_cmd_start = 0;
static char S_outdir[256];
static int S_press_hold = 0;
static int S_should_close = 0;

static struct timespec S_real0;

static uint64_t real_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)(t.tv_sec - S_real0.tv_sec) * 1000 + (t.tv_nsec - S_real0.tv_nsec) / 1000000;
}

static int btn_from_name(const char *s)
{
    if (!strcmp(s, "cross") || !strcmp(s, "x")) return BTN_CROSS;
    if (!strcmp(s, "circle") || !strcmp(s, "o")) return BTN_CIRCLE;
    if (!strcmp(s, "square")) return BTN_SQUARE;
    if (!strcmp(s, "triangle")) return BTN_TRIANGLE;
    if (!strcmp(s, "up")) return BTN_UP;
    if (!strcmp(s, "down")) return BTN_DOWN;
    if (!strcmp(s, "left")) return BTN_LEFT;
    if (!strcmp(s, "right")) return BTN_RIGHT;
    if (!strcmp(s, "l1")) return BTN_L1;
    if (!strcmp(s, "r1")) return BTN_R1;
    if (!strcmp(s, "l2")) return BTN_L2;
    if (!strcmp(s, "r2")) return BTN_R2;
    if (!strcmp(s, "options")) return BTN_OPTIONS;
    if (!strcmp(s, "share")) return BTN_SHARE;
    if (!strcmp(s, "ps")) return BTN_PS;
    return 0;
}

static void load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "dump: cannot open script %s\n", path);
        exit(1);
    }
    char line[256];
    while (fgets(line, sizeof(line), f) && S_ncmd < 128) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == 0) continue;
        char word[32], arg[64];
        if (sscanf(p, "%31s %63[^\n]", word, arg) < 1) continue;
        for (char *q = arg; *q; q++) if (*q == '\n' || *q == '\r') *q = 0;
        Cmd *c = &S_cmds[S_ncmd++];
        memset(c, 0, sizeof(*c));
        if (!strcmp(word, "wait"))   { c->type = CMD_WAIT;  c->a = atoi(arg); }
        else if (!strcmp(word, "press")) { c->type = CMD_PRESS; psa_strcpy(c->s, sizeof(c->s), arg); }
        else if (!strcmp(word, "move"))  { c->type = CMD_MOVE; sscanf(arg, "%d %d", &c->a, &c->b); }
        else if (!strcmp(word, "shot"))  { c->type = CMD_SHOT; psa_strcpy(c->s, sizeof(c->s), arg); }
        else if (!strcmp(word, "type"))  { c->type = CMD_TYPE; psa_strcpy(c->s, sizeof(c->s), arg); }
        else if (!strcmp(word, "sel"))   { c->type = CMD_SEL;  c->a = atoi(arg); }
        else if (!strcmp(word, "exit"))  { c->type = CMD_EXIT; }
        else S_ncmd--;
    }
    fclose(f);
}

int video_init(int argc, char **argv)
{
    const char *script = NULL;
    psa_strcpy(S_outdir, sizeof(S_outdir), "shots");
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--outdir") && i + 1 < argc) psa_strcpy(S_outdir, sizeof(S_outdir), argv[++i]);
    }
    if (!script) {
        fprintf(stderr, "usage: %s --script <file> [--outdir <dir>]\n", argv[0]);
        return 1;
    }
    load_script(script);
    ensure_dir(S_outdir);
    clock_gettime(CLOCK_MONOTONIC, &S_real0);
    static uint32_t fb[PSA_W * PSA_H];   /* 8.3 MB in .bss */
    gfx_init(fb, PSA_W, PSA_H);
    return 0;
}

static void exec_frame(void)
{
    if (S_press_hold > 0 && --S_press_hold == 0)
        g_in.buttons = 0;

    if (S_cur >= S_ncmd) { S_should_close = 1; return; }
    Cmd *c = &S_cmds[S_cur];
    switch (c->type) {
    case CMD_WAIT:
        if (g_vclock - S_cmd_start >= (uint64_t)c->a) { S_cur++; S_cmd_start = g_vclock; }
        break;
    case CMD_PRESS: {
        if (S_press_hold == 0) {
            g_in.buttons = btn_from_name(c->s);
            S_press_hold = 2;      /* hold for 2 frames so the click edge registers */
        } else {
            S_cur++; S_cmd_start = g_vclock;
        }
        break;
    }
    case CMD_MOVE:
        g_in.mx = c->a; g_in.my = c->b;
        S_cur++; S_cmd_start = g_vclock;
        break;
    case CMD_TYPE:
        if (!osk_active()) { S_cur++; S_cmd_start = g_vclock; }
        else {
            osk_inject_str(c->s);
            S_cur++; S_cmd_start = g_vclock;
        }
        break;
    case CMD_SHOT: {
        char path[512];
        path_join(path, sizeof(path), S_outdir, c->s);
        size_t pl = strlen(path);
        if (pl + 4 < sizeof(path)) strcat(path, ".png");
        /* fb is ARGB8888 (bytes B,G,R,A) - swizzle to RGBA for PNG */
        static unsigned char rgba[PSA_W * PSA_H * 4];
        const uint32_t *src = gfx_fb();
        for (int i = 0; i < PSA_W * PSA_H; i++) {
            uint32_t px = src[i];
            rgba[i * 4 + 0] = (unsigned char)((px >> 16) & 0xFF);
            rgba[i * 4 + 1] = (unsigned char)((px >> 8) & 0xFF);
            rgba[i * 4 + 2] = (unsigned char)(px & 0xFF);
            rgba[i * 4 + 3] = 255;   /* framebuffer alpha is meaningless on video out */
        }
        stbi_write_png(path, PSA_W, PSA_H, 4, rgba, PSA_W * 4);
        printf("dump: wrote %s\n", path);
        S_cur++; S_cmd_start = g_vclock;
        break;
    }
    case CMD_SEL:
        ui_set_sel(c->a);
        S_cur++; S_cmd_start = g_vclock;
        break;
    case CMD_EXIT:
        S_should_close = 1;
        break;
    }
}

void video_poll(void)
{
    /* virtual clock runs 20x real time so animations complete quickly */
    static uint64_t last_real = 0;
    uint64_t r = real_ms();
    if (last_real) g_vclock += (r - last_real) * 20;
    else g_vclock += 16 * 20;
    last_real = r;

    int prev = g_in.buttons;
    exec_frame();
    g_in.prev = prev;
    g_in.clicked = g_in.buttons & ~prev;
    g_in.released = prev & ~g_in.buttons;
}

void video_present(void) { /* framebuffer kept in memory; PNG via shot */ }

int video_should_close(void) { return S_should_close || g_quit_request; }

void video_shutdown(void) { }
