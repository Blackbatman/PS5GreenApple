/* icons.c - procedural vector icons incl. Apple logo (SVG path flatten + even-odd fill) */
#include "icons.h"
#include "gfx.h"
#include <math.h>

/* ---------- Apple logo: simple-icons 24x24 path (CC0-like free use) ----------
 * M12.152 6.896c-.948 0-2.415-1.078-3.96-1.04-2.04.027-3.91 1.183-4.961 3.014
 * -2.117 3.675-.546 9.103 1.519 12.09 1.013 1.454 2.208 3.09 3.792 3.03 1.52
 * -.065 2.09-.987 3.935-.987 1.831 0 2.35.987 3.96.948 1.637-.026 2.676-1.48
 * 3.676-2.948 1.156-1.688 1.636-3.325 1.662-3.415-.039-.013-3.182-1.221-3.22
 * -4.857-.026-3.04 2.48-4.494 2.597-4.559-1.429-2.09-3.623-2.324-4.377-2.376
 * -2-.156-3.675 1.09-4.61 1.09zM15.53 3.83c.843-1.012 1.4-2.427 1.245-3.83
 * -1.207.052-2.662.805-3.532 1.818-.78.896-1.454 2.338-1.273 3.714 1.338.104
 * 2.715-.688 3.559-1.701
 */
#define MAXPTS 1024
static float P_x[MAXPTS], P_y[MAXPTS];
static int   P_n;

static void emit(float x, float y)
{
    if (P_n >= MAXPTS) return;
    P_x[P_n] = x; P_y[P_n] = y; P_n++;
}

static float lastx, lasty;

static void cubic(float x1, float y1, float x2, float y2, float x3, float y3)
{
    float x0 = lastx, y0 = lasty;
    for (int i = 1; i <= 16; i++) {
        float t = (float)i / 16.f;
        float mt = 1.f - t;
        float a = mt * mt * mt, b = 3 * mt * mt * t, c = 3 * mt * t * t, d = t * t * t;
        emit(a * x0 + b * x1 + c * x2 + d * x3, a * y0 + b * y1 + c * y2 + d * y3);
    }
    lastx = x3; lasty = y3;
}

/* number parser for svg path */
static const char *sp;
static int read_num(float *out)
{
    const char *p = sp;
    while (*p == ' ' || *p == ',') p++;
    int neg = 0;
    float v = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;
    if (*p == '.') {                 /* forms like "-.948" */
        p++;
        float f = 0.1f;
        while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * f; f *= 0.1f; }
        sp = p;
        *out = neg ? -v : v;
        return 1;
    }
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') v = v * 10.f + (*p++ - '0');
    if (*p == '.') {
        p++;
        float f = 0.1f;
        while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * f; f *= 0.1f; }
    }
    sp = p;
    *out = neg ? -v : v;
    return 1;
}

static void flatten_apple(void)
{
    /* Parse with a tiny state machine over the known path.
     * Commands present: M (abs), m (rel), c (rel cubic), l (implicit via m),
     * z. Numbers separated by spaces/commas. */
    float x = 0, y = 0;
    char cmd = 0;
    P_n = 0;
    lastx = lasty = 0;

    static const char *path =
        "M12.152 6.896c-.948 0-2.415-1.078-3.96-1.04-2.04.027-3.91 1.183-4.961 3.014"
        "-2.117 3.675-.546 9.103 1.519 12.09 1.013 1.454 2.208 3.09 3.792 3.03 1.52"
        "-.065 2.09-.987 3.935-.987 1.831 0 2.35.987 3.96.948 1.637-.026 2.676-1.48"
        " 3.676-2.948 1.156-1.688 1.636-3.325 1.662-3.415-.039-.013-3.182-1.221-3.22"
        "-4.857-.026-3.04 2.48-4.494 2.597-4.559-1.429-2.09-3.623-2.324-4.377-2.376"
        "-2-.156-3.675 1.09-4.61 1.09zM15.53 3.83c.843-1.012 1.4-2.427 1.245-3.83"
        "-1.207.052-2.662.805-3.532 1.818-.78.896-1.454 2.338-1.273 3.714 1.338.104"
        " 2.715-.688 3.559-1.701z";

    sp = path;
    int first_m = 1;
    while (*sp) {
        while (*sp == ' ' || *sp == ',') sp++;
        if (*sp == 0) break;
        if ((*sp >= 'a' && *sp <= 'z') || (*sp >= 'A' && *sp <= 'Z')) {
            cmd = *sp++;
            if (cmd == 'M' || cmd == 'm') {
                float nx, ny;
                if (!read_num(&nx)) break;
                if (!read_num(&ny)) break;
                x = (cmd == 'm') ? x + nx : nx;
                y = (cmd == 'm') ? y + ny : ny;
                lastx = x; lasty = y;
                if (first_m) { emit(x, y); first_m = 0; }
                else emit(x, y);
                /* implicit lineto sequence follows for M/m */
                cmd = (cmd == 'm') ? 'l' : 'L';
                continue;
            } else if (cmd == 'L' || cmd == 'l') {
                float nx, ny;
                if (!read_num(&nx)) break;
                if (!read_num(&ny)) break;
                x = (cmd == 'l') ? x + nx : nx;
                y = (cmd == 'l') ? y + ny : ny;
                emit(x, y);
                lastx = x; lasty = y;
                continue;
            } else if (cmd == 'C' || cmd == 'c') {
                float x1, y1, x2, y2, x3, y3;
                if (!read_num(&x1)) break;
                if (!read_num(&y1)) break;
                if (!read_num(&x2)) break;
                if (!read_num(&y2)) break;
                if (!read_num(&x3)) break;
                if (!read_num(&y3)) break;
                if (cmd == 'c') { x1 += x; y1 += y; x2 += x; y2 += y; x3 += x; y3 += y; }
                cubic(x1, y1, x2, y2, x3, y3);
                x = x3; y = y3;
                continue;
            } else if (cmd == 'z' || cmd == 'Z') {
                continue; /* subpath closed: even-odd fill handles it */
            }
        } else {
            /* implicit repetition of last command */
            if (cmd == 'L' || cmd == 'l') {
                float nx, ny;
                if (!read_num(&nx)) break;
                if (!read_num(&ny)) break;
                x = (cmd == 'l') ? x + nx : nx;
                y = (cmd == 'l') ? y + ny : ny;
                emit(x, y);
                lastx = x; lasty = y;
            } else if (cmd == 'c' || cmd == 'C') {
                float x1, y1, x2, y2, x3, y3;
                if (!read_num(&x1)) break;
                if (!read_num(&y1)) break;
                if (!read_num(&x2)) break;
                if (!read_num(&y2)) break;
                if (!read_num(&x3)) break;
                if (!read_num(&y3)) break;
                if (cmd == 'c') { x1 += x; y1 += y; x2 += x; y2 += y; x3 += x; y3 += y; }
                cubic(x1, y1, x2, y2, x3, y3);
                x = x3; y = y3;
            } else break;
        }
    }
    /* transform to screen coords: 24x24 space -> requested */
}

void icons_draw_apple(int cx, int cy, float size, uint32_t c)
{
    static int inited = 0;
    if (!inited) { flatten_apple(); inited = 1; }
    if (P_n < 3) return;
    float scale = size / 24.f;
    static float tx[MAXPTS], ty[MAXPTS];
    for (int i = 0; i < P_n; i++) {
        tx[i] = cx + (P_x[i] - 12.f) * scale;
        ty[i] = cy + (P_y[i] - 12.f) * scale;
    }
    gfx_poly_fill(tx, ty, P_n, c);
}

/* ---------- app icons ---------- */

static float icon_sdf(float px, float py, float bx, float by, float r)
{
    float qx = fabsf(px) - bx + r, qy = fabsf(py) - by + r;
    float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0;
    return sqrtf(ax * ax + ay * ay) + (qx > 0 && qy > 0 ? 0 : (qx > qy ? qx : qy)) - r;
}

static void round_grad(int x, int y, int s, uint32_t top, uint32_t bot)
{
    float r = s * 0.24f, bx = s * 0.5f, by = s * 0.5f;
    for (int yy = 0; yy < s; yy++) {
        uint32_t cc = lerp_color(top, bot, (float)yy / s);
        for (int xx = 0; xx < s; xx++) {
            float d = icon_sdf(xx + 0.5f - bx, yy + 0.5f - by, bx, by, r);
            float cov = clampf(0.5f - d, 0.f, 1.f);
            if (cov > 0.f)
                gfx_blend_pixel(x + xx, y + yy, mul_alpha(cc, (int)(cov * 255.f)));
        }
    }
    gfx_round_rect_outline(x, y, s, s, (int)r, 1, RGBA(0, 0, 0, 60));
}

void icons_draw_app_icon(IconKind kind, int x, int y, int size, int selected)
{
    int s = size;
    switch (kind) {
    case ICON_FINDER: {
        round_grad(x, y, s, RGB(90, 190, 255), RGB(20, 120, 220));
        /* face */
        int fx = x + s / 2;
        gfx_rect(fx - 1, y + s / 5, 2, (int)(s * 0.18f), RGB(20, 60, 110));
        gfx_rect(x + (int)(s * 0.62f), y + s / 5, 2, (int)(s * 0.18f), RGB(20, 60, 110));
        /* smile */
        for (int i = -1; i <= 1; i++)
            gfx_pixel(fx + i * (s / 6), y + (int)(s * 0.62f) + (i == 0 ? 2 : 0), RGB(20, 60, 110));
        gfx_hline(x + (int)(s * 0.3f), x + (int)(s * 0.7f), y + (int)(s * 0.66f), RGB(20, 60, 110));
        break;
    }
    case ICON_TERMINAL: {
        round_grad(x, y, s, RGB(48, 48, 52), RGB(12, 12, 14));
        int m = s / 5;
        gfx_rect(x + m, y + m, s - 2 * m, s - 2 * m, RGB(8, 8, 10));
        gfx_line(x + m + s / 8, y + m + s / 6, x + m + s / 8 + s / 8, y + m + s / 6 + s / 8, RGB(80, 220, 100));
        gfx_line(x + m + s / 8, y + m + s / 6, x + m + s / 8 + s / 8, y + m + s / 6 - s / 10 + s / 8, RGB(80, 220, 100));
        gfx_hline(x + m + s / 4, x + s - m - s / 8, y + m + (int)(s * 0.45f), RGB(200, 200, 210));
        break;
    }
    case ICON_TEXTEDIT: {
        round_grad(x, y, s, RGB(250, 250, 252), RGB(215, 215, 222));
        int m = s / 5;
        for (int i = 0; i < 4; i++)
            gfx_hline(x + m, x + s - m, y + m + (int)(s * 0.14f) * (i + 1) + i, RGB(160, 160, 175));
        break;
    }
    case ICON_ABOUT:
        round_grad(x, y, s, RGB(80, 80, 88), RGB(30, 30, 36));
        icons_draw_apple(x + s / 2, y + s / 2, s * 0.56f, RGB(240, 240, 248));
        break;
    case ICON_SETTINGS: {
        round_grad(x, y, s, RGB(130, 130, 140), RGB(70, 70, 80));
        icons_draw_glyph(ICON_GEAR, x + s / 2, y + s / 2, (int)(s * 0.62f), RGB(235, 235, 245));
        break;
    }
    case ICON_BOOTCAMP: {
        round_grad(x, y, s, RGB(70, 90, 120), RGB(20, 30, 45));
        int h = y + s / 2;
        /* half-and-half: windows green + linux orange hint */
        gfx_rect(x + s / 5, y + s / 5, (int)(s * 0.26f), (int)(s * 0.26f), RGB(120, 220, 130));
        gfx_rect(x + s / 5, h + s / 10, (int)(s * 0.26f), (int)(s * 0.26f), RGB(255, 190, 90));
        icons_draw_glyph(ICON_WIN, x + (int)(s * 0.68f), y + s / 2, (int)(s * 0.42f), RGB(220, 235, 255));
        break;
    }
    case ICON_TRASH: {
        round_grad(x, y, s, RGB(90, 95, 105), RGB(45, 48, 55));
        icons_draw_glyph(ICON_TRASH, x + s / 2, y + s / 2, (int)(s * 0.6f), RGB(235, 235, 245));
        break;
    }
    default:
        round_grad(x, y, s, RGB(120, 120, 130), RGB(60, 60, 70));
        break;
    }
    if (selected) {
        int r = (int)(s * 0.26f) + 6;
        gfx_round_rect_outline(x - 6, y - 6, s + 12, s + 12, r, 3, RGB(10, 132, 255));
    }
}

void icons_draw_glyph(IconKind kind, int cx, int cy, int size, uint32_t c)
{
    int h = size / 2;
    switch (kind) {
    case ICON_WIN:
        gfx_rect(cx - h, cy - h, h - 2, h - 2, c);
        gfx_rect(cx + 2, cy - h, h - 2, h - 2, c);
        gfx_rect(cx - h, cy + 2, h - 2, h - 2, c);
        gfx_rect(cx + 2, cy + 2, h - 2, h - 2, c);
        break;
    case ICON_LINUX:
        gfx_circle_fill(cx, cy - h / 3, h / 2, c);
        gfx_rect_outline(cx - h / 2, cy - h / 4, h, h / 2, 2, c);
        gfx_rect(cx - h / 2, cy + h / 3, h, h / 3, c);
        break;
    case ICON_GEAR: {
        gfx_circle_outline(cx, cy, h - 3, 4, c);
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159f / 4.f;
            int x0 = cx + (int)((h - 1) * cosf(a)), y0 = cy + (int)((h - 1) * sinf(a));
            int x1 = cx + (int)((h + 3) * cosf(a)), y1 = cy + (int)((h + 3) * sinf(a));
            gfx_line(x0, y0, x1, y1, c);
            gfx_line(x0 + 1, y0, x1 + 1, y1, c);
        }
        gfx_circle_fill(cx, cy, h / 3, c);
        break;
    }
    case ICON_FOLDER:
        gfx_rect(cx - h, cy - h / 2, h, h / 3, c);
        gfx_rect(cx - h, cy - h / 6, 2 * h, (int)(h * 0.9f), c);
        break;
    case ICON_FILE:
        gfx_rect(cx - h / 2, cy - h, h, 2 * h, RGB(235, 235, 240));
        gfx_rect_outline(cx - h / 2, cy - h, h, 2 * h, 1, RGB(120, 120, 130));
        for (int i = 0; i < 4; i++)
            gfx_hline(cx - h / 2 + 3, cx + h / 2 - 3, cy - h + 5 + i * 5, RGB(150, 150, 160));
        break;
    case ICON_IMAGE:
        gfx_rect_outline(cx - h, cy - h / 2, 2 * h, h + h / 2, 2, c);
        gfx_circle_fill(cx - h / 3, cy - h / 6, 3, c);
        gfx_line(cx - h + 4, cy + h / 2 - 4, cx - 2, cy - h / 4, c);
        gfx_line(cx - 6, cy + h / 4, cx + h - 4, cy + h / 2 - 6, c);
        break;
    case ICON_TRASH:
        gfx_rect(cx - h / 2, cy - h / 3, h, (int)(h * 1.4f), c);
        gfx_rect(cx - h / 2 - 2, cy - h / 2, h + 4, 3, c);
        gfx_rect(cx - 3, cy - h / 2 - 4, 6, 4, c);
        break;
    case ICON_WIFI:
        for (int i = 1; i <= 3; i++)
            gfx_circle_outline(cx, cy + h / 2, (int)(h * 0.35f * i) + 2, 2, c);
        gfx_circle_fill(cx, cy + h / 2 - 1, 2, c);
        break;
    case ICON_BATTERY: {
        gfx_rect_outline(cx - h, cy - h / 3, 2 * h - 4, (int)(h * 0.66f), 2, c);
        gfx_rect(cx + h - 3, cy - h / 6, 3, h / 3, c);
        gfx_rect(cx - h + 2, cy - h / 3 + 2, (int)(h * 1.1f), (int)(h * 0.66f) - 4, c);
        break;
    }
    case ICON_CC:
        gfx_circle_outline(cx, cy, h, 2, c);
        gfx_circle_fill(cx - h / 2, cy, h / 3, c);
        break;
    case ICON_SEARCH:
        gfx_circle_outline(cx - 2, cy - 2, h - 2, 2, c);
        gfx_line(cx + h / 2 - 1, cy + h / 2 - 1, cx + h + 1, cy + h + 1, c);
        break;
    case ICON_CHEVRON_R:
        gfx_line(cx - h / 3, cy - h / 2, cx + h / 3, cy, c);
        gfx_line(cx + h / 3, cy, cx - h / 3, cy + h / 2, c);
        break;
    case ICON_DOT:
        gfx_circle_fill(cx, cy, h, c);
        break;
    default:
        break;
    }
}
