/* font.c - glyph cache + utf8 shaping-free renderer (stb_truetype) */
#include "font.h"
#include "gfx.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

extern const unsigned char font_sans_reg_start[], font_sans_reg_end[];
extern const unsigned char font_sans_bold_start[], font_sans_bold_end[];
extern const unsigned char font_mono_start[], font_mono_end[];

typedef struct Glyph {
    struct Glyph *next;
    unsigned cp;
    float size;
    int glyph_index;
    int w, h, xoff, yoff, adv;
    uint8_t *bits;
} Glyph;

typedef struct Face {
    const unsigned char *data;
    int len;
    int loaded;
    stbtt_fontinfo info;
    float ascent, descent, linegap;   /* scaled for 1px */
    Glyph *cache;
} Face;

static Face S_faces[FONT_COUNT];

int font_init(void)
{
    static const unsigned char *datas[FONT_COUNT] = {
        font_sans_reg_start, font_sans_bold_start, font_mono_start
    };
    static const unsigned char *ends[FONT_COUNT] = {
        font_sans_reg_end, font_sans_bold_end, font_mono_end
    };
    for (int i = 0; i < FONT_COUNT; i++) {
        Face *f = &S_faces[i];
        f->data = datas[i];
        f->len = (int)(ends[i] - datas[i]);
        f->cache = NULL;
        f->loaded = stbtt_InitFont(&f->info, f->data, 0) != 0;
        if (f->loaded) {
            float scale = stbtt_ScaleForPixelHeight(&f->info, 1.0f);
            int asc = 0, desc = 0, gap = 0;
            stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
            f->ascent = asc * scale;
            f->descent = desc * scale;
            f->linegap = gap * scale;
        }
    }
    return 1;
}

void font_shutdown(void)
{
    for (int i = 0; i < FONT_COUNT; i++) {
        Glyph *g = S_faces[i].cache;
        while (g) {
            Glyph *nx = g->next;
            free(g->bits);
            free(g);
            g = nx;
        }
        S_faces[i].cache = NULL;
        S_faces[i].loaded = 0;
    }
}

static Glyph *get_glyph(int id, unsigned cp, float size)
{
    if (id < 0 || id >= FONT_COUNT || !S_faces[id].loaded) return NULL;
    Face *f = &S_faces[id];
    for (Glyph *g = f->cache; g; g = g->next)
        if (g->cp == cp && g->size == size) return g;

    float scale = stbtt_ScaleForPixelHeight(&f->info, size);
    int gi = stbtt_FindGlyphIndex(&f->info, (int)cp);
    if (gi <= 0) gi = 0;

    Glyph *g = (Glyph *)calloc(1, sizeof(Glyph));
    if (!g) return NULL;
    g->cp = cp;
    g->size = size;
    g->bits = stbtt_GetGlyphBitmap(&f->info, 0, scale, gi, &g->w, &g->h, &g->xoff, &g->yoff);
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&f->info, gi, &adv, &lsb);
    g->adv = (int)(adv * scale);
    g->glyph_index = gi;
    g->next = f->cache;
    f->cache = g;
    return g;
}

int font_utf8_next(const char *s, unsigned *out_cp)
{
    const unsigned char *p = (const unsigned char *)s;
    if (!*p) return 0;
    unsigned cp = 0;
    int n = 0;
    if (*p < 0x80)                    { cp = *p; n = 1; }
    else if ((*p & 0xE0) == 0xC0)     { cp = *p & 0x1F; n = 2; }
    else if ((*p & 0xF0) == 0xE0)     { cp = *p & 0x0F; n = 3; }
    else                              { cp = *p & 0x07; n = 4; }
    for (int i = 1; i < n && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
    *out_cp = cp;
    return n;
}

int font_draw(int id, const char *s, int x, int y, float size, uint32_t c)
{
    if (!s || id < 0 || id >= FONT_COUNT) return x;
    Face *f = &S_faces[id];
    if (!f->loaded) return x;
    float scale = stbtt_ScaleForPixelHeight(&f->info, size);
    int baseline = y + (int)(f->ascent * size);
    int cx = x;
    for (const char *p = s; *p; ) {
        unsigned cp;
        int n = font_utf8_next(p, &cp);
        if (n <= 0) break;
        p += n;
        if (cp == '\t') cp = ' ';
        if (cp == '\n') continue;
        Glyph *g = get_glyph(id, cp, size);
        if (!g) continue;
        if (g->bits && g->w > 0 && g->h > 0) {
            for (int yy = 0; yy < g->h; yy++) {
                const uint8_t *row = g->bits + (size_t)yy * g->w;
                for (int xx = 0; xx < g->w; xx++) {
                    int a = row[xx];
                    if (a > 0)
                        gfx_blend_pixel(cx + g->xoff + xx, baseline + g->yoff + yy, mul_alpha(c, a));
                }
            }
        }
        /* kerning with next cp */
        cx += g->adv;
        if (*p) {
            unsigned ncp;
            if (font_utf8_next(p, &ncp)) {
                int ngi = stbtt_FindGlyphIndex(&f->info, (int)ncp);
                int k = stbtt_GetGlyphKernAdvance(&f->info, g->glyph_index, ngi);
                cx += (int)(k * scale);
            }
        }
    }
    return cx;
}

int font_text_w(int id, const char *s, float size)
{
    if (!s || id < 0 || id >= FONT_COUNT || !S_faces[id].loaded) return 0;
    int w = 0;
    for (const char *p = s; *p; ) {
        unsigned cp;
        int n = font_utf8_next(p, &cp);
        if (n <= 0) break;
        p += n;
        if (cp == '\t') cp = ' ';
        if (cp == '\n') continue;
        Glyph *g = get_glyph(id, cp, size);
        if (g) w += g->adv;
    }
    return w;
}

int font_line_h(int id, float size)
{
    if (id < 0 || id >= FONT_COUNT) return (int)size;
    if (!S_faces[id].loaded) return (int)size;
    Face *f = &S_faces[id];
    return (int)((f->ascent - f->descent) * size) + 1;
}

int font_draw_string(int x, int y, const char *str, uint32_t color)
{
    /* canonical text-layer API: UI face at the default UI size */
    return font_draw(FONT_UI, str, x, y, FONT_UI_SIZE, color);
}
