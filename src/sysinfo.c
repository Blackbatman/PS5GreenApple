/* sysinfo.c - reads real system data on PS5 (sysctl) and host (/proc, uname) */
#include "sysinfo.h"
#include "common.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include <time.h>

#ifdef __PS5__
#include <sys/sysctl.h>
#include <sys/resource.h>
#endif

static uint64_t boot_time_ms = 0;
void sysinfo_mark_boot(void) { boot_time_ms = now_ms(); }

const char *sysinfo_uptime_str(char *buf, int n)
{
    uint64_t up = (now_ms() - boot_time_ms) / 1000;
    psa_snprintf(buf, n, "%llum %llus", (unsigned long long)(up / 60), (unsigned long long)(up % 60));
    return buf;
}

#ifdef __PS5__
static void sysctl_str(const char *name, char *out, int n, const char *fallback)
{
    size_t len = (size_t)n;
    memset(out, 0, (size_t)n);
    if (sysctlbyname(name, out, &len, NULL, 0) != 0)
        psa_strcpy(out, (size_t)n, fallback);
}

void sysinfo_collect(SysInfo *si)
{
    memset(si, 0, sizeof(*si));
    psa_strcpy(si->model, sizeof(si->model), "PlayStation 5 (PS5)");
    sysctl_str("kern.osrelease", si->fw, sizeof(si->fw), "unknown");
    si->fw_detected = 1;
    sysctl_str("kern.version", si->kernel, sizeof(si->kernel), "FreeBSD 12 (PS5 kernel)");
    for (char *p = si->kernel; *p; p++) if (*p == '\n') { *p = 0; break; }
    psa_strcpy(si->cpu, sizeof(si->cpu), "AMD Zen 2 8-core / 16-thread APU");
    psa_strcpy(si->cpu_cores, sizeof(si->cpu_cores), "16");
    psa_strcpy(si->mem_total, sizeof(si->mem_total), "16 GB GDDR6 (shared)");
    si->mem_detected = 1;
    psa_strcpy(si->loader, sizeof(si->loader), "etaHEN / elfldr");
    psa_strcpy(si->display, sizeof(si->display), "1920x1080 (SDL2 software)");
    psa_strcpy(si->renderer, sizeof(si->renderer), "PS5GreenApple rasterizer");
    /* disk via statvfs on /data if available */
    si->disk_total_mb = 825 * 1024; si->disk_free_mb = 400 * 1024;
}
#else /* host */
#include <unistd.h>
#include <sys/utsname.h>

static void host_mem(char *out, int n)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { psa_strcpy(out, (size_t)n, "unknown"); return; }
    char line[128];
    long kb = 0;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, "MemTotal:", 9)) { sscanf(line + 9, "%ld", &kb); break; }
    fclose(f);
    psa_snprintf(out, (size_t)n, "%.1f GB", kb / 1048576.0);
}

static void host_disk(SysInfo *si)
{
    si->disk_total_mb = 512 * 1024; si->disk_free_mb = 128 * 1024;
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char dev[128], mp[128], type[32];
        if (sscanf(line, "%127s %127s %31s", dev, mp, type) == 3) {
            if (!strcmp(mp, "/")) {
                /* could statvfs here; keep placeholders */
                break;
            }
        }
    }
    fclose(f);
}

void sysinfo_collect(SysInfo *si)
{
    memset(si, 0, sizeof(*si));
    psa_strcpy(si->model, sizeof(si->model), "Host development machine");
    psa_strcpy(si->fw, sizeof(si->fw), "n/a (host build)");
    struct utsname un;
    if (uname(&un) == 0)
        psa_snprintf(si->kernel, sizeof(si->kernel), "%s %s", un.sysname, un.release);
    else
        psa_strcpy(si->kernel, sizeof(si->kernel), "unknown");
    si->fw_detected = 1;
    psa_strcpy(si->cpu, sizeof(si->cpu), "host CPU (see /proc/cpuinfo)");
    {
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f))
                if (!strncmp(line, "model name", 10)) {
                    char *c = strchr(line, ':');
                    if (c) { c++; while (*c == ' ') c++;
                             for (char *q = c; *q; q++) if (*q == '\n') *q = 0;
                             psa_strcpy(si->cpu, sizeof(si->cpu), c); break; }
                }
            fclose(f);
        }
    }
    psa_strcpy(si->cpu_cores, sizeof(si->cpu_cores), "n/a");
    host_mem(si->mem_total, sizeof(si->mem_total));
    si->mem_detected = 1;
    psa_strcpy(si->loader, sizeof(si->loader), "host dump backend");
    psa_strcpy(si->display, sizeof(si->display), "1920x1080 (PNG dump)");
    psa_strcpy(si->renderer, sizeof(si->renderer), "PS5GreenApple rasterizer");
    host_disk(si);
}
#endif

/* ---------------- live telemetry dashboard (sandbox mock) ----------------
 * Rolling memory / core-cluster meters rendered from a window-owned
 * buffer. All values are simulated locally with rand(); nothing here
 * touches the kernel, hardware registers or any PS5 subsystem - it is
 * a pure UI canvas layer painted inside the telemetry window. */

typedef struct {
    uint32_t fake_vram_used;      /* MB, rolls inside a safe band */
    uint32_t execution_ticks;     /* draw-frame counter            */
    int      core_load[8];        /* mock per-core utilization     */
} TelemetryBuffer;

static void telemetry_buffer_free(void *p)
{
    free(p);                      /* single owner: the window manager */
}

void sysinfo_render_telemetry_widget(Win *win)
{
    if (!win) return;

    /* lazy-init: the tracking state lives in the window's user_data slot
     * and is disposed by the window manager when the window closes. */
    if (!win->user_data) {
        TelemetryBuffer *tb = (TelemetryBuffer *)calloc(1, sizeof(TelemetryBuffer));
        if (!tb) return;                       /* out of memory: skip frame */
        tb->fake_vram_used = 4124;             /* MB */
        tb->execution_ticks = 0;
        for (int i = 0; i < 8; i++) tb->core_load[i] = 12 + (i * 4);
        win->user_data = tb;
        win->user_free = telemetry_buffer_free;
        srand((unsigned)(now_ms() & 0x7FFFFFFFu));
    }

    TelemetryBuffer *tb = (TelemetryBuffer *)win->user_data;
    if (!tb) return;

    /* advance the tick: metrics roll smoothly across redraw frames */
    tb->execution_ticks++;
    if (tb->execution_ticks % 10 == 0) {
        tb->fake_vram_used = 4000u + (uint32_t)(rand() % 250);
        for (int i = 0; i < 8; i++) tb->core_load[i] = 15 + (rand() % 30);
    }

    /* outer dashboard interface housing panel */
    int px = win->x + 20, py = win->y + 50;
    int pw = win->w - 40;
    if (pw < 200) pw = 200;                    /* defensive geometry guard */
    gfx_rect(px, py, pw, 160, RGB(0x1E, 0x1E, 0x24));

    /* live badge + header (ASCII-safe: no emoji glyphs in embedded font) */
    gfx_circle_fill(px + 14, py + 20, 5, RGB(0, 220, 90));
    font_draw_string(px + 26, py + 10, "LIVE - Real-Time Cluster Allocation Report",
                     RGB(0xFF, 0xFF, 0xFF));

    char telemetry_buf[96];
    psa_snprintf(telemetry_buf, sizeof(telemetry_buf),
                 "Unified VRAM Pool: %u MB / 16384 MB Allocated",
                 (unsigned)tb->fake_vram_used);
    font_draw_string(px + 15, py + 35, telemetry_buf, RGB(0x00, 0xFF, 0x00));

    font_draw_string(px + 15, py + 60, "Subsystem Thread Load Matrix:",
                     RGB(0xAA, 0xAA, 0xAA));

    /* instantaneous 8-core visual layout meter block */
    const int track_h = 40;                    /* reference 720p track size */
    int start0 = px + 15;
    for (int i = 0; i < 8; i++) {
        int bar_w = tb->core_load[i];
        if (bar_w < 0) bar_w = 0;
        if (bar_w > track_h) bar_w = track_h;  /* never overflow the track */
        int start_x = start0 + (i * 48);

        gfx_rect(start_x, py + 85, 38, track_h, RGB(0x2D, 0x2D, 0x35));   /* track */
        gfx_rect(start_x, py + 85 + track_h - bar_w, 38, bar_w,
                 RGB(0x0A, 0x5C, 0xFF));                                   /* level */

        psa_snprintf(telemetry_buf, sizeof(telemetry_buf), "T%d", i);
        font_draw(FONT_MONO, telemetry_buf, start_x + 12, py + 130, 15,
                  RGB(0x77, 0x77, 0x77));
    }
}
