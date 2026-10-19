/* main.c - PS5GreenApple entry point: robust boot + event dispatcher loop
 *
 * Pipeline (each stage guarded, fail-closed):
 *   1. display backend init  (SDL2 on PS5 / PNG-dump on host)  -> fatal on error
 *   2. desktop subsystem init (fonts, window manager, theme, boot anim)
 *   3. single-frame loop: poll events -> sandboxed frame dispatch -> present
 *   4. graceful disposal: fonts, windows state, backend, zero dangling refs
 *
 * The loop never touches bare-metal state: every event routes into the
 * unprivileged UI sandbox (desktop_frame -> wm/ui/gfx layers only).
 */
#include "common.h"
#include "desktop.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    /* 1. display backend: SDL2 video + DualSense pads on PS5, scripted
     *    PNG dump on host. Any failure here is fatal and cleanly reported. */
    if (video_init(argc, argv) != 0) {
        fprintf(stderr, "ps5greenapple: display backend init failed - aborting\n");
        return EXIT_FAILURE;
    }

    /* 2. desktop subsystem: embedded font atlas, window manager, theme,
     *    boot animation state. font_init() degrades gracefully (faces that
     *    fail to load simply render no glyphs), so only a hard failure of
     *    the subsystem bootstrap itself would abort here. */
    desktop_init();

    /* 3. core single-frame event dispatcher: hardware input is polled by
     *    the active backend, dispatched through the sandboxed desktop
     *    frame (state machine + window compositing), then presented. */
    while (!video_should_close()) {
        video_poll();
        desktop_frame();
        video_present();
    }

    /* 4. graceful resource disposal - release glyph caches and window
     *    state, then shut the backend down so nothing outlives main(). */
    font_shutdown();
    video_shutdown();
    fprintf(stderr, "ps5greenapple: environment closed cleanly, all contexts released\n");
    return EXIT_SUCCESS;
}
