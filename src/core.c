/* core.c - shared utils + input state */
#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>

InputState g_in;

int in_pressed(int mask)  { return (g_in.buttons & mask) != 0; }
int in_clicked(int mask)  { return (g_in.clicked & mask) != 0; }

uint64_t now_ms(void)
{
    /* g_vclock lives in the backend; exported there via symbol */
    extern uint64_t g_vclock;
    return g_vclock;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerpf(float a, float b, float t)    { return a + (b - a) * t; }
float ease_out(float t)                   { t = clampf(t, 0.f, 1.f); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }

uint32_t mul_alpha(uint32_t c, int alpha)
{
    int a = (C_A(c) * alpha) / 255;
    if (a > 255) a = 255;
    return RGBA(C_R(c), C_G(c), C_B(c), a);
}

uint32_t lerp_color(uint32_t a, uint32_t b, float t)
{
    t = clampf(t, 0.f, 1.f);
    return RGBA((int)lerpf(C_R(a), C_R(b), t),
                (int)lerpf(C_G(a), C_G(b), t),
                (int)lerpf(C_B(a), C_B(b), t),
                (int)lerpf(C_A(a), C_A(b), t));
}

void path_join(char *out, size_t n, const char *a, const char *b)
{
    if (!a || !*a) { psa_strcpy(out, n, b ? b : ""); return; }
    if (!b || !*b) { psa_strcpy(out, n, a); return; }
    if (a[strlen(a) - 1] == '/') psa_snprintf(out, n, "%s%s", a, b);
    else psa_snprintf(out, n, "%s/%s", a, b);
}

void ensure_dir(const char *p)
{
#if defined(_WIN32)
    _mkdir(p);
#else
    mkdir(p, 0755);
#endif
}

int file_exists(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

long file_size(const char *p)
{
    FILE *f = fopen(p, "rb");
    long sz = -1;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) == 0) sz = ftell(f);
    fclose(f);
    return sz;
}

void psa_strcpy(char *dst, size_t n, const char *src)
{
    if (!dst || n == 0) return;
    if (!src) src = "";
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
}

void psa_snprintf(char *dst, size_t n, const char *fmt, ...)
{
    va_list ap;
    if (!dst || n == 0) return;
    va_start(ap, fmt);
    vsnprintf(dst, n, fmt, ap);
    va_end(ap);
}
