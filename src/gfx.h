/* gfx.h - software ARGB8888 rasterizer (no GPU) */
#ifndef GFX_H
#define GFX_H
#include "common.h"

void     gfx_init(uint32_t *fb, int w, int h);
uint32_t *gfx_fb(void);
int      gfx_w(void);
int      gfx_h(void);

void     gfx_clear(uint32_t c);
void     gfx_pixel(int x, int y, uint32_t c);
void     gfx_blend_pixel(int x, int y, uint32_t c);   /* respects c's alpha */
void     gfx_rect(int x, int y, int w, int h, uint32_t c);
void     gfx_rect_outline(int x, int y, int w, int h, int th, uint32_t c);
void     gfx_round_rect(int x, int y, int w, int h, int r, uint32_t c);       /* filled, AA */
void     gfx_round_rect_outline(int x, int y, int w, int h, int r, int th, uint32_t c);
void     gfx_hline(int x0, int x1, int y, uint32_t c);
void     gfx_vline(int x, int y0, int y1, uint32_t c);
void     gfx_line(int x0, int y0, int x1, int y1, uint32_t c);
void     gfx_circle_fill(int cx, int cy, int r, uint32_t c);
void     gfx_circle_outline(int cx, int cy, int r, int th, uint32_t c);
void     gfx_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void     gfx_glow(int cx, int cy, float r, uint32_t c, float strength);       /* gaussian blob */
void     gfx_poly_fill(const float *xs, const float *ys, int n, uint32_t c);  /* even-odd */
void     gfx_push_clip(int x, int y, int w, int h);
void     gfx_pop_clip(void);

#endif
