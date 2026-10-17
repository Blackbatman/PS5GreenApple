/* PS5GreenApple - common definitions
 * macOS Sonoma-style desktop environment for jailbroken PlayStation 5.
 * Built with ps5-payload-sdk + SDL2. Renders with a pure-software
 * ARGB8888 rasterizer (no GPU/shader dependency).
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define PSA_VERSION      "1.0.0"
#define PSA_APP_NAME     "PS5GreenApple"
#define PSA_W            1920
#define PSA_H            1080

/* Persistent data location (settings, notes, linux boot files) */
#ifdef __PS5__
#  define PSA_DATA_DIR   "/data/PS5GreenApple"
#else
#  define PSA_DATA_DIR   "./data"
#endif

/* ---- colors: 0xAARRGGBB ---- */
#define RGB(r,g,b)       ((uint32_t)(0xFF000000u | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(b)))
#define RGBA(r,g,b,a)    ((uint32_t)(((uint32_t)(a)<<24) | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(b)))
#define C_R(c)           (int)(((c)>>16)&0xFF)
#define C_G(c)           (int)(((c)>>8)&0xFF)
#define C_B(c)           (int)((c)&0xFF)
#define C_A(c)           (int)(((c)>>24)&0xFF)

/* ---- DualSense buttons (bitmask) ---- */
enum {
    BTN_CROSS     = 1 << 0,
    BTN_CIRCLE    = 1 << 1,
    BTN_SQUARE    = 1 << 2,
    BTN_TRIANGLE  = 1 << 3,
    BTN_L1        = 1 << 4,
    BTN_R1        = 1 << 5,
    BTN_L2        = 1 << 6,
    BTN_R2        = 1 << 7,
    BTN_UP        = 1 << 8,
    BTN_DOWN      = 1 << 9,
    BTN_LEFT      = 1 << 10,
    BTN_RIGHT     = 1 << 11,
    BTN_OPTIONS   = 1 << 12,
    BTN_SHARE     = 1 << 13,
    BTN_PS        = 1 << 14,
    BTN_TOUCHPAD  = 1 << 15,
    BTN_L3        = 1 << 16,
    BTN_R3        = 1 << 17,
};

/* ---- shared input state, filled by the active backend ---- */
typedef struct InputState {
    int mx, my;          /* pointer position (informational) */
    int buttons;         /* current mask */
    int prev;            /* previous frame mask */
    int clicked;         /* rising edge this frame */
    int released;        /* falling edge this frame */
    int idle_ms;         /* ms since last activity */
} InputState;

extern InputState g_in;
extern int g_quit_request;   /* set 1 to exit the main loop (desktop.c) */

int      in_pressed(int mask);
int      in_clicked(int mask);

/* Virtual clock in ms (real time on PS5, accelerated under the dump backend) */
uint64_t now_ms(void);

/* ---- small utils (core.c) ---- */
float    clampf(float v, float lo, float hi);
float    lerpf(float a, float b, float t);
float    ease_out(float t);            /* 1-(1-t)^3 */
uint32_t lerp_color(uint32_t a, uint32_t b, float t);
uint32_t mul_alpha(uint32_t c, int alpha);
void     path_join(char *out, size_t n, const char *a, const char *b);
void     ensure_dir(const char *p);
int      file_exists(const char *p);
long     file_size(const char *p);
void     psa_strcpy(char *dst, size_t n, const char *src);
void     psa_snprintf(char *dst, size_t n, const char *fmt, ...);

/* ---- backend interface (implemented by backend_sdl2.c or backend_dump.c) ---- */
int      video_init(int argc, char **argv);   /* returns 0 on success */
void     video_poll(void);                    /* update input + virtual clock */
void     video_present(void);
int      video_should_close(void);
void     video_shutdown(void);

#endif /* COMMON_H */
