# Architecture

PS5GreenApple is a single-process SDL2 payload that renders a desktop with a
pure-software rasterizer. No GPU/SCELF graphics APIs are used — every pixel
is written into one 1920×1080 ARGB8888 framebuffer.

```
main.c
  ├── backend_sdl2.c   (PS5 / desktop: video-out surface + DualSense)
  │     or backend_dump.c (headless: virtual clock + scripted PNG shots)
  └── desktop.c        frame loop, global state machine
        ├── boot.c         Apple-logo boot animation (skippable)
        ├── wm.c           window list, mac/win/linux chrome, z-order
        ├── ui.c           hotspot list + D-pad focus navigation
        ├── osk.c          on-screen keyboard (writes into caller buffers)
        ├── icons.c        procedural vector icons + flattened Apple logo
        ├── font.c         stb_truetype glyph cache (fonts .incbin'd in fonts.S)
        ├── gfx.c          rasterizer: clip stack, AA rounded rects, gradients,
        │                  polygon even-odd fill, gaussian glow
        └── app_*.c        Finder / Terminal / TextEdit / About / Settings /
                           Boot Camp (each owns one window + state)
```

## Frame model

Each iteration of the main loop:

1. `video_poll()` — the backend updates `g_in` (button mask + rising/falling
   edges). On the dump backend this is driven by a script and a 20× virtual
   clock, so the identical UI code produces deterministic screenshots.
2. `desktop_frame()` — clears the hotspot list, dispatches global input
   (OPTIONS power dialog, CIRCLE back, SQUARE save, L1/R1 scroll, D-pad
   `ui_nav`), then draws: cached wallpaper → menu bar → windows → dock →
   dialogs → on-screen keyboard → toast → selection ring.
3. `video_present()` — SDL surface update (or PNG capture).

Apps never poll the controller directly; they either register hotspots and
query `ui_pressed(id)`, or check edge functions for shortcuts. This keeps
the input model identical for controller and scripted input.

## Memory

Everything is static or stack — the only heap allocations are the wallpaper
cache (8.3 MB) and glyph bitmaps. The payload runs comfortably inside the
homebrew memory budget on all supported firmwares.

## Porting notes

- The renderer is endianness-explicit little-endian ARGB8888, matching
  SceVideoOut's default swap format via the SDL surface.
- `backend_dump.c` exists so UI development never requires a console: the
  whole desktop, including the boot animation and the OSK, is exercised by
  `scripts/demo-script.txt` (`make shots`).
- To add an app: implement `open()` + a `WinDraw`, add a `DockEntry`, give it
  window/app ids following the ranges in `scripts/demo-script.txt`.
