/* make_icon.c - render sce_sys/icon0.png (512x512) with the project's own
 * software rasterizer + font stack. Zero external art assets: the same
 * gfx/font code that draws the desktop draws the icon.
 *
 * Host-only tool:  make icon   (see Makefile)
 * Output: build/icon0.png
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "gfx.h"
#include "font.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#define IW 512
#define IH 512
#define CORNER 96

static uint32_t fb[IW * IH];

uint64_t g_vclock = 0;   /* backend clock stub - static render, never advances */

/* multiply the icon's alpha by rounded-rect coverage (radius CORNER) */
static void round_corners(void)
{
    for (int y = 0; y < IH; y++) {
        for (int x = 0; x < IW; x++) {
            float cov = 1.0f;
            int cx = (x < CORNER) ? CORNER : (x >= IW - CORNER) ? IW - 1 - CORNER : -1;
            int cy = (y < CORNER) ? CORNER : (y >= IH - CORNER) ? IH - 1 - CORNER : -1;
            if (cx >= 0 && cy >= 0) {
                float dx = (float)(x - cx), dy = (float)(y - cy);
                float d = (float)CORNER - sqrtf(dx * dx + dy * dy);
                cov = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
            }
            uint32_t *px = &fb[y * IW + x];
            if (cov <= 0.0f) { *px = 0; continue; }
            if (cov < 1.0f) {
                unsigned a = (unsigned)((((*px) >> 24) & 0xFF) * cov);
                *px = (*px & 0x00FFFFFFu) | ((uint32_t)a << 24);
            }
        }
    }
}

static void draw_centered(int font_id, const char *s, float size, int y, uint32_t c)
{
    int w = font_text_w(font_id, s, size);
    font_draw(font_id, s, (IW - w) / 2, y, size, c);
}

int main(int argc, char **argv)
{
    const char *out = (argc > 1) ? argv[1] : "build/icon0.png";

    gfx_init(fb, IW, IH);

    /* Sonoma-style deep gradient backdrop + two soft auroras */
    gfx_vgrad(0, 0, IW, IH, RGB(10, 18, 34), RGB(18, 48, 56));
    gfx_glow(150, 140, 200.0f, RGB(0, 190, 160), 0.32f);
    gfx_glow(390, 400, 180.0f, RGB(0, 120, 220), 0.28f);

    /* faint energy ring behind the mark */
    gfx_circle_outline(256, 225, 150, 2, RGBA(110, 255, 220, 45));

    /* green apple: shadow, body, volume, highlight, rim */
    gfx_circle_fill(256, 252, 126, RGBA(0, 10, 0, 60));
    gfx_circle_fill(256, 232, 118, RGB(56, 184, 88));
    gfx_circle_fill(256, 244, 100, RGB(42, 148, 70));
    gfx_circle_fill(216, 188, 34, RGB(126, 232, 148));
    gfx_circle_fill(208, 180, 15, RGB(196, 255, 210));
    gfx_circle_outline(256, 232, 118, 3, RGBA(0, 255, 190, 100));

    /* stem + leaf */
    gfx_rect(250, 92, 12, 44, RGB(90, 60, 34));
    gfx_circle_fill(296, 106, 24, RGB(52, 168, 72));
    gfx_circle_fill(290, 100, 10, RGB(120, 220, 140));

    /* wordmark */
    font_init();   /* per project convention: degrades gracefully, no check */
    draw_centered(FONT_BOLD, "GreenApple", 40.0f, 386, RGB(240, 250, 245));
    draw_centered(FONT_UI,   "homebrew desktop for PS5", 20.0f, 438, RGB(168, 196, 206));
    draw_centered(FONT_MONO, "FW 3.00-13.60 / payload loaders", 15.0f, 468, RGB(120, 150, 160));
    font_shutdown();

    round_corners();

    /* ARGB8888 (bytes B,G,R,A) -> RGBA bytes for PNG, keep real alpha */
    static unsigned char rgba[IW * IH * 4];
    for (int i = 0; i < IW * IH; i++) {
        uint32_t px = fb[i];
        rgba[i * 4 + 0] = (unsigned char)((px >> 16) & 0xFF);
        rgba[i * 4 + 1] = (unsigned char)((px >> 8) & 0xFF);
        rgba[i * 4 + 2] = (unsigned char)(px & 0xFF);
        rgba[i * 4 + 3] = (unsigned char)((px >> 24) & 0xFF);
    }
    if (!stbi_write_png(out, IW, IH, 4, rgba, IW * 4)) {
        fprintf(stderr, "make_icon: cannot write %s\n", out);
        return 1;
    }
    printf("make_icon: wrote %s (%dx%d, alpha rounded r=%d)\n", out, IW, IH, CORNER);
    return 0;
}
