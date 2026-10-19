/* boot.c - black screen, fading Apple logo + progress bar (X skips) */
#include "boot.h"
#include "gfx.h"
#include "font.h"
#include "icons.h"
#include "common.h"

static uint64_t S_t0;
static int S_active;
/* timeline: fade-in 0-1200ms, hold, bar 1400-3000ms, fade-out 3000-3400ms */
#define BOOT_TOTAL 3400

void boot_reset(void) { S_t0 = now_ms(); S_active = 1; }
int  boot_active(void) { return S_active; }
void boot_skip(void)   { S_active = 0; }

int boot_frame(void)
{
    if (!S_active) return 1;
    uint64_t t = now_ms() - S_t0;
    if (in_clicked(BTN_CROSS) || t >= BOOT_TOTAL) { S_active = 0; return 1; }

    gfx_clear(RGB(0, 0, 0));

    /* logo alpha */
    float a;
    if (t < 1200)      a = ease_out((float)t / 1200.f);
    else if (t < 3000) a = 1.f;
    else               a = 1.f - (float)(t - 3000) / 400.f;
    a = clampf(a, 0.f, 1.f);

    icons_draw_apple(PSA_W / 2, PSA_H / 2 - 60, 150.f, mul_alpha(RGB(245, 245, 250), (int)(a * 255)));

    /* progress bar */
    if (t > 1400) {
        float bp = clampf((float)(t - 1400) / 1600.f, 0.f, 1.f);
        int bw = 320, bx = (PSA_W - bw) / 2, by = PSA_H / 2 + 120;
        gfx_round_rect(bx, by, bw, 6, 3, RGBA(255, 255, 255, (int)(40 * a)));
        gfx_round_rect(bx, by, (int)(bw * bp), 6, 3, mul_alpha(RGB(235, 235, 245), (int)(200 * a)));
    }

    if (t > 2400 && t < 3300) {
        const char *hint = "Press X to skip";
        int alpha = (int)(110 * a);
        /* canonical text-layer API (font.h contract) */
        font_draw_string(PSA_W / 2 - font_text_w(FONT_UI, hint, FONT_UI_SIZE) / 2,
                         PSA_H - 120, hint, mul_alpha(RGB(160, 160, 175), alpha));
    }
    return 0;
}
