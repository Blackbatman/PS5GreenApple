/* backend_sdl2.c - PS5 (and desktop Linux) SDL2 backend
 *
 * On PS5 the payload-sdk SDL2 port maps a 1920x1080 ARGB8888 surface to
 * SceVideoOut directly; we render straight into the window surface.
 * DualSense is exposed through SDL_GameController.
 */
#include "common.h"
#include "gfx.h"
#include <SDL.h>

uint64_t g_vclock = 0;
static SDL_Window *S_win;
static SDL_Surface *S_surf;
static int S_own_fb = 0;      /* host fallback when surface format differs */
static uint32_t *S_fb;
static int S_should_close = 0;
static SDL_GameController *S_pad;

static int btn_map(int b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A:             return BTN_CROSS;
    case SDL_CONTROLLER_BUTTON_B:             return BTN_CIRCLE;
    case SDL_CONTROLLER_BUTTON_X:             return BTN_SQUARE;
    case SDL_CONTROLLER_BUTTON_Y:             return BTN_TRIANGLE;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return BTN_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_R1;
    case SDL_CONTROLLER_BUTTON_START:         return BTN_OPTIONS;
    case SDL_CONTROLLER_BUTTON_BACK:          return BTN_SHARE;
    case SDL_CONTROLLER_BUTTON_GUIDE:         return BTN_PS;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD:      return BTN_TOUCHPAD;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return BTN_L3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return BTN_R3;
    default: return 0;
    }
}

int video_init(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* 1. video + controller subsystems; fail-closed with SDL diagnostics */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    /* 2. unprivileged user-space display window (HiDPI aware on desktop) */
    S_win = SDL_CreateWindow("PS5GreenApple - macOS Environment Simulation Port",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             PSA_W, PSA_H,
                             SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!S_win) { fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError()); return 1; }
    /* 3. bind the software rasterizer to the window surface. The pure
     *    software canvas is the intended path (no GPU/shader dependency);
     *    if the surface is unavailable we fall back to an owned buffer
     *    and keep rendering so the environment never hard-locks. */
    S_surf = SDL_GetWindowSurface(S_win);
    if (!S_surf)
        fprintf(stderr, "Warning: window surface unavailable (%s) - using owned canvas\n",
                SDL_GetError());

    if (S_surf && S_surf->format && S_surf->format->format == SDL_PIXELFORMAT_ARGB8888 &&
        S_surf->pitch == PSA_W * 4) {
        S_fb = (uint32_t *)S_surf->pixels;   /* render directly into the surface */
    } else {
        S_fb = (uint32_t *)malloc(sizeof(uint32_t) * PSA_W * PSA_H);
        S_own_fb = 1;
    }
    if (!S_fb) return 1;
    gfx_init(S_fb, PSA_W, PSA_H);

    if (SDL_NumGameControllers() > 0)
        S_pad = SDL_GameControllerOpen(0);
    return 0;
}

static void poll_pad(void)
{
    int mask = 0;
    if (SDL_NumGameControllers() > 0) {
        if (!S_pad) S_pad = SDL_GameControllerOpen(0);
        if (S_pad) {
            for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
                if (SDL_GameControllerGetButton(S_pad, (SDL_GameControllerButton)b))
                    mask |= btn_map(b);
            /* dpad via hat-style buttons */
            if (SDL_GameControllerGetButton(S_pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    mask |= BTN_UP;
            if (SDL_GameControllerGetButton(S_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  mask |= BTN_DOWN;
            if (SDL_GameControllerGetButton(S_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  mask |= BTN_LEFT;
            if (SDL_GameControllerGetButton(S_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) mask |= BTN_RIGHT;
            /* triggers as buttons */
            if (SDL_GameControllerGetAxis(S_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  > 12000) mask |= BTN_L2;
            if (SDL_GameControllerGetAxis(S_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000) mask |= BTN_R2;
            /* left stick moves the pointer (informational) */
            static int mx = PSA_W / 2, my = PSA_H / 2;
            int lx = SDL_GameControllerGetAxis(S_pad, SDL_CONTROLLER_AXIS_LEFTX);
            int ly = SDL_GameControllerGetAxis(S_pad, SDL_CONTROLLER_AXIS_LEFTY);
            mx += abs(lx) > 4000 ? lx / 12000 : 0;
            my += abs(ly) > 4000 ? ly / 12000 : 0;
            if (mx < 0) mx = 0; if (mx >= PSA_W) mx = PSA_W - 1;
            if (my < 0) my = 0; if (my >= PSA_H) my = PSA_H - 1;
            g_in.mx = mx; g_in.my = my;
        }
    }
    g_in.buttons = mask;
}

void video_poll(void)
{
    static uint64_t last = 0;
    uint64_t now = SDL_GetTicks();
    g_vclock = now;

    int prev = g_in.buttons;
    int mask = 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT: S_should_close = 1; break;
        case SDL_CONTROLLERDEVICEADDED: if (!S_pad) S_pad = SDL_GameControllerOpen(ev.cdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: if (S_pad) { SDL_GameControllerClose(S_pad); S_pad = NULL; } break;
        case SDL_KEYDOWN: {
            int b = 0;
            switch (ev.key.keysym.sym) {
            case SDLK_RETURN: case SDLK_KP_ENTER: b = BTN_CROSS; break;
            case SDLK_BACKSPACE: case SDLK_ESCAPE: b = BTN_CIRCLE; break;
            case SDLK_s: b = BTN_SQUARE; break;
            case SDLK_t: b = BTN_TRIANGLE; break;
            case SDLK_q: b = BTN_L1; break;
            case SDLK_e: b = BTN_R1; break;
            case SDLK_F1: b = BTN_OPTIONS; break;
            case SDLK_UP: b = BTN_UP; break;
            case SDLK_DOWN: b = BTN_DOWN; break;
            case SDLK_LEFT: b = BTN_LEFT; break;
            case SDLK_RIGHT: b = BTN_RIGHT; break;
            }
            mask |= b;
            break;
        }
        default: break;
        }
    }
    poll_pad();
    mask |= g_in.buttons;

    g_in.prev = prev;
    g_in.buttons = mask;
    g_in.clicked = mask & ~prev;
    g_in.released = prev & ~mask;

    /* ~60 fps cap */
    if (now - last < 15) SDL_Delay(15 - (now - last));
    last = now;
}

void video_present(void)
{
    if (!S_win) return;
    if (S_own_fb && S_surf) {
        /* copy fb -> surface (handles differing formats/pitch) */
        if (SDL_MUSTLOCK(S_surf)) SDL_LockSurface(S_surf);
        uint32_t *dst = (uint32_t *)S_surf->pixels;
        int pitch = S_surf->pitch / 4;
        int cw = (S_surf->w < PSA_W) ? S_surf->w : PSA_W;   /* never write past a surface row */
        for (int y = 0; y < PSA_H && y < S_surf->h; y++)
            memcpy(dst + (size_t)y * pitch, S_fb + (size_t)y * PSA_W, (size_t)cw * 4);
        if (SDL_MUSTLOCK(S_surf)) SDL_UnlockSurface(S_surf);
    }
    if (S_surf)
        SDL_UpdateWindowSurface(S_win);
}

int video_should_close(void) { return S_should_close || g_quit_request; }

void video_shutdown(void)
{
    /* orderly teardown: pad -> framebuffer -> window -> subsystem */
    if (S_pad) { SDL_GameControllerClose(S_pad); S_pad = NULL; }
    if (S_own_fb && S_fb) { free(S_fb); S_fb = NULL; }
    S_own_fb = 0;
    if (S_win) { SDL_DestroyWindow(S_win); S_win = NULL; }
    S_surf = NULL;
    SDL_Quit();
}
