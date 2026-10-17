/* gfx.c - software rasterizer, ARGB8888, clipping + AA rounded rects */
#include "gfx.h"
#include <math.h>

static uint32_t *S_fb;
static int S_w, S_h;

#define MAX_CLIP 8
typedef struct { int x, y, w, h; } Clip;
static Clip S_clip[MAX_CLIP];
static int S_clip_n = 0;

static int cur_clip_x(void) { return S_clip_n ? S_clip[S_clip_n - 1].x : 0; }
static int cur_clip_y(void) { return S_clip_n ? S_clip[S_clip_n - 1].y : 0; }
static int cur_clip_x2(void) { return S_clip_n ? S_clip[S_clip_n - 1].x + S_clip[S_clip_n - 1].w : S_w; }
static int cur_clip_y2(void) { return S_clip_n ? S_clip[S_clip_n - 1].y + S_clip[S_clip_n - 1].h : S_h; }

void gfx_init(uint32_t *fb, int w, int h) { S_fb = fb; S_w = w; S_h = h; S_clip_n = 0; }
uint32_t *gfx_fb(void) { return S_fb; }
int gfx_w(void) { return S_w; }
int gfx_h(void) { return S_h; }

void gfx_push_clip(int x, int y, int w, int h)
{
    if (S_clip_n >= MAX_CLIP) return;
    if (S_clip_n) {
        Clip *p = &S_clip[S_clip_n - 1];
        int x2 = x + w, y2 = y + h;
        if (x < p->x) x = p->x;
        if (y < p->y) y = p->y;
        if (x2 > p->x + p->w) x2 = p->x + p->w;
        if (y2 > p->y + p->h) y2 = p->y + p->h;
        w = x2 - x; h = y2 - y;
    }
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    S_clip[S_clip_n].x = x; S_clip[S_clip_n].y = y;
    S_clip[S_clip_n].w = w; S_clip[S_clip_n].h = h;
    S_clip_n++;
}

void gfx_pop_clip(void) { if (S_clip_n > 0) S_clip_n--; }

void gfx_blend_pixel(int x, int y, uint32_t c)
{
    int a = C_A(c);
    if (x < cur_clip_x() || y < cur_clip_y() || x >= cur_clip_x2() || y >= cur_clip_y2()) return;
    if (a <= 0) return;
    if (a >= 255) { S_fb[(size_t)y * S_w + x] = c | 0xFF000000u; return; }
    uint32_t *d = &S_fb[(size_t)y * S_w + x];
    int ia = 255 - a;
    int r = (C_R(c) * a + C_R(*d) * ia) / 255;
    int g = (C_G(c) * a + C_G(*d) * ia) / 255;
    int b = (C_B(c) * a + C_B(*d) * ia) / 255;
    *d = RGBA(r, g, b, 255);
}

void gfx_pixel(int x, int y, uint32_t c)
{
    if (x < cur_clip_x() || y < cur_clip_y() || x >= cur_clip_x2() || y >= cur_clip_y2()) return;
    S_fb[(size_t)y * S_w + x] = c;
}

void gfx_clear(uint32_t c)
{
    for (int i = 0; i < S_w * S_h; i++) S_fb[i] = c;
}

void gfx_rect(int x, int y, int w, int h, uint32_t c)
{
    int x0 = x > cur_clip_x() ? x : cur_clip_x();
    int y0 = y > cur_clip_y() ? y : cur_clip_y();
    int x1 = x + w < cur_clip_x2() ? x + w : cur_clip_x2();
    int y1 = y + h < cur_clip_y2() ? y + h : cur_clip_y2();
    int a = C_A(c);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = &S_fb[(size_t)yy * S_w];
        if (a >= 255) {
            for (int xx = x0; xx < x1; xx++) row[xx] = c;
        } else {
            int ia = 255 - a;
            int cr = C_R(c), cg = C_G(c), cb = C_B(c);
            for (int xx = x0; xx < x1; xx++) {
                uint32_t d = row[xx];
                row[xx] = RGBA((cr * a + C_R(d) * ia) / 255,
                               (cg * a + C_G(d) * ia) / 255,
                               (cb * a + C_B(d) * ia) / 255, 255);
            }
        }
    }
}

void gfx_rect_outline(int x, int y, int w, int h, int th, uint32_t c)
{
    gfx_rect(x, y, w, th, c);
    gfx_rect(x, y + h - th, w, th, c);
    gfx_rect(x, y, th, h, c);
    gfx_rect(x + w - th, y, th, h, c);
}

void gfx_hline(int x0, int x1, int y, uint32_t c)
{
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    gfx_rect(x0, y, x1 - x0 + 1, 1, c);
}

void gfx_vline(int x, int y0, int y1, uint32_t c)
{
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    gfx_rect(x, y0, 1, y1 - y0 + 1, c);
}

void gfx_line(int x0, int y0, int x1, int y1, uint32_t c)
{
    /* Bresenham */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_circle_fill(int cx, int cy, int r, uint32_t c)
{
    int y0 = cy - r, y1 = cy + r;
    if (y0 < cur_clip_y()) y0 = cur_clip_y();
    if (y1 > cur_clip_y2()) y1 = cur_clip_y2();
    for (int y = y0; y < y1; y++) {
        int dy = y - cy;
        int span = (int)sqrtf((float)(r * r - dy * dy));
        int x0 = cx - span, x1 = cx + span + 1;
        if (x0 < cur_clip_x()) x0 = cur_clip_x();
        if (x1 > cur_clip_x2()) x1 = cur_clip_x2();
        uint32_t *row = &S_fb[(size_t)y * S_w];
        for (int x = x0; x < x1; x++) row[x] = c;
    }
}

void gfx_circle_outline(int cx, int cy, int r, int th, uint32_t c)
{
    int ri = r - th;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            if (d <= (float)r && d > (float)ri)
                gfx_blend_pixel(cx + dx, cy + dy, c);
        }
    }
}

/* signed distance of rounded rect: negative inside */
static float rrect_sdf(float px, float py, float bx, float by, float r)
{
    float qx = fabsf(px) - bx + r, qy = fabsf(py) - by + r;
    float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0;
    return sqrtf(ax * ax + ay * ay) + (qx > 0 && qy > 0 ? 0 : (qx > qy ? qx : qy)) - r;
}

void gfx_round_rect(int x, int y, int w, int h, int r, uint32_t c)
{
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    int x0 = x - 2, y0 = y - 2, x1 = x + w + 2, y1 = y + h + 2;
    if (x0 < cur_clip_x()) x0 = cur_clip_x();
    if (y0 < cur_clip_y()) y0 = cur_clip_y();
    if (x1 > cur_clip_x2()) x1 = cur_clip_x2();
    if (y1 > cur_clip_y2()) y1 = cur_clip_y2();

    float bx = w * 0.5f, by = h * 0.5f;
    float ccx = x + bx, ccy = y + by;

    /* solid interior (fast path), then AA border ring */
    if (r > 0) {
        gfx_rect(x + 1, y + r, w - 2, h - 2 * r, c);
        gfx_rect(x + r, y + 1, w - 2 * r, r - 1, c);
        gfx_rect(x + r, y + h - r, w - 2 * r, r - 1, c);
    } else {
        gfx_rect(x, y, w, h, c);
        return;
    }
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            float d = rrect_sdf(xx + 0.5f - ccx, yy + 0.5f - ccy, bx, by, (float)r);
            if (d < -0.5f) continue;               /* interior already filled */
            float cov = clampf(0.5f - d, 0.f, 1.f);
            if (cov <= 0) continue;
            gfx_blend_pixel(xx, yy, mul_alpha(c, (int)(cov * 255.f)));
        }
    }
}

void gfx_round_rect_outline(int x, int y, int w, int h, int r, int th, uint32_t c)
{
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    float bx = w * 0.5f, by = h * 0.5f;
    float ccx = x + bx, ccy = y + by;
    int x0 = x - 2, y0 = y - 2, x1 = x + w + 2, y1 = y + h + 2;
    if (x0 < cur_clip_x()) x0 = cur_clip_x();
    if (y0 < cur_clip_y()) y0 = cur_clip_y();
    if (x1 > cur_clip_x2()) x1 = cur_clip_x2();
    if (y1 > cur_clip_y2()) y1 = cur_clip_y2();
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            float d = rrect_sdf(xx + 0.5f - ccx, yy + 0.5f - ccy, bx, by, (float)r);
            if (d > 0.5f || d < -(float)th - 0.5f) continue;
            float cov = clampf(0.5f - fabsf(d + th * 0.5f) - 0.5f + 1.0f, 0.f, 1.f);
            /* simple band coverage */
            cov = (d <= 0.f && d >= -th) ? 1.f : clampf(0.5f - fabsf(d + (d > 0 ? 0.f : (float)th)), 0.f, 1.f);
            if (cov <= 0) continue;
            gfx_blend_pixel(xx, yy, mul_alpha(c, (int)(cov * 255.f)));
        }
    }
}

void gfx_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot)
{
    for (int i = 0; i < h; i++) {
        uint32_t c = lerp_color(top, bot, (float)i / (float)(h > 1 ? h - 1 : 1));
        gfx_rect(x, y + i, w, 1, c);
    }
}

void gfx_glow(int cx, int cy, float r, uint32_t c, float strength)
{
    int x0 = (int)(cx - r), x1 = (int)(cx + r);
    int y0 = (int)(cy - r), y1 = (int)(cy + r);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > S_w) x1 = S_w;
    if (y1 > S_h) y1 = S_h;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float dx = x - cx, dy = y - cy;
            float d2 = (dx * dx + dy * dy) / (r * r);
            if (d2 > 1.f) continue;
            float f = expf(-d2 * 4.0f) * strength;
            uint32_t *d = &S_fb[(size_t)y * S_w + x];
            int a = (int)(C_A(c) * f);
            if (a <= 0) continue;
            if (a > 255) a = 255;
            int ia = 255 - a;
            int rr = (C_R(c) * a + C_R(*d) * ia) / 255;
            int gg = (C_G(c) * a + C_G(*d) * ia) / 255;
            int bb = (C_B(c) * a + C_B(*d) * ia) / 255;
            *d = RGBA(rr, gg, bb, 255);
        }
    }
}

void gfx_poly_fill(const float *xs, const float *ys, int n, uint32_t c)
{
    if (n < 3) return;
    float miny = ys[0], maxy = ys[0];
    for (int i = 1; i < n; i++) {
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }
    int iy0 = (int)floorf(miny), iy1 = (int)ceilf(maxy);
    if (iy0 < cur_clip_y()) iy0 = cur_clip_y();
    if (iy1 > cur_clip_y2()) iy1 = cur_clip_y2();
    static float xints[256];
    for (int y = iy0; y < iy1; y++) {
        float cy = y + 0.5f;
        int ni = 0;
        for (int i = 0; i < n && ni < 256; i++) {
            int j = (i + 1) % n;
            float y1 = ys[i], y2 = ys[j];
            if ((cy >= y1 && cy < y2) || (cy >= y2 && cy < y1)) {
                float t = (cy - y1) / (y2 - y1);
                xints[ni++] = xs[i] + t * (xs[j] - xs[i]);
            }
        }
        /* sort insertion (n small) */
        for (int a = 1; a < ni; a++) {
            float v = xints[a]; int b = a - 1;
            while (b >= 0 && xints[b] > v) { xints[b + 1] = xints[b]; b--; }
            xints[b + 1] = v;
        }
        for (int i = 0; i + 1 < ni; i += 2) {
            int xa = (int)ceilf(xints[i] - 0.5f), xb = (int)floorf(xints[i + 1] - 0.5f);
            if (xa < cur_clip_x()) xa = cur_clip_x();
            if (xb > cur_clip_x2()) xb = cur_clip_x2();
            for (int x = xa; x < xb; x++) gfx_blend_pixel(x, y, c);
        }
    }
}
