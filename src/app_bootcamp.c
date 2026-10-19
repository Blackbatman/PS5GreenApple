/* app_bootcamp.c - Boot Camp: dual/tri-boot detection + OS chrome showcase
 *
 * Scans the filesystem for Linux boot files (bzImage/initramfs) and Windows
 * partitions (ntfs/exfat mounts), and demonstrates the three window chrome
 * styles by opening preview shells.
 */
#include "apps.h"
#include "wm.h"
#include "gfx.h"
#include "font.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "desktop.h"
#include "common.h"
#include <sys/stat.h>

typedef struct {
    int  linux_ok, win_ok;
    char bz[256], initrd[256], winpart[256];
    long bz_size, initrd_size;
    char wintype[32];
} BC;

static BC S_bc;
static void bc_draw(Win *w);   /* forward decl */

static void bc_detect(void)
{
    memset(&S_bc, 0, sizeof(S_bc));
    static const char *bz_paths[] = {
        "/data/PS5GreenApple/linux/bzImage", "/mnt/usb0/boot/bzImage",
        "/mnt/usb1/boot/bzImage", "/boot/bzImage", "./linux/bzImage", NULL
    };
    static const char *ird_paths[] = {
        "/data/PS5GreenApple/linux/initramfs.img", "/mnt/usb0/boot/initramfs.img",
        "/mnt/usb1/boot/initramfs.img", "/boot/initramfs.img", "./linux/initramfs.img", NULL
    };
    for (int i = 0; bz_paths[i]; i++)
        if (file_exists(bz_paths[i])) {
            psa_strcpy(S_bc.bz, sizeof(S_bc.bz), bz_paths[i]);
            S_bc.bz_size = file_size(bz_paths[i]);
            break;
        }
    for (int i = 0; ird_paths[i]; i++)
        if (file_exists(ird_paths[i])) {
            psa_strcpy(S_bc.initrd, sizeof(S_bc.initrd), ird_paths[i]);
            S_bc.initrd_size = file_size(ird_paths[i]);
            break;
        }
    S_bc.linux_ok = S_bc.bz[0] && S_bc.initrd[0];

    /* windows partition: look for ntfs/exfat mounts */
    FILE *f = fopen("/proc/mounts", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char dev[128], mp[128], type[32];
            if (sscanf(line, "%127s %127s %31s", dev, mp, type) == 3) {
                if (!strcmp(type, "ntfs") || !strcmp(type, "ntfs3") ||
                    !strcmp(type, "exfat") || !strcmp(type, "vfat")) {
                    psa_snprintf(S_bc.winpart, sizeof(S_bc.winpart), "%s on %s", dev, mp);
                    psa_strcpy(S_bc.wintype, sizeof(S_bc.wintype), type);
                    S_bc.win_ok = 1;
                    break;
                }
            }
        }
        fclose(f);
    }
}

void app_bootcamp_open(void)
{
    bc_detect();
    if (!wm_find(WIN_BOOTCAMP))
        wm_open(WIN_BOOTCAMP, "Boot Camp", CHROME_MAC, 400, 120, 1120, 700, bc_draw);
    else
        wm_raise(wm_find(WIN_BOOTCAMP));
}

static void win_shell_draw(Win *w);
static void linux_shell_draw(Win *w);

static void status_line(char *out, int n, int ok, const char *what, const char *detail)
{
    psa_snprintf(out, (size_t)n, "%s %s%s%s", ok ? "[FOUND]" : "[  -  ]", what,
                 detail && detail[0] ? ": " : "", detail ? detail : "");
}

static void bc_draw(Win *w)
{
    int x = w->x + 30, yy = w->y + 62;
    int cw = w->w - 60;

    font_draw(FONT_BOLD, "Detected bootable systems", x, yy, 21, g_theme.text_bright);
    yy += 40;

    char line[512];
    gfx_round_rect(x, yy, cw, 150, 12, RGB(38, 38, 44));
    icons_draw_glyph(ICON_LINUX, x + 44, yy + 75, 44, RGB(255, 190, 90));
    int lx = x + 84;
    status_line(line, sizeof(line), S_bc.bz[0], "bzImage", S_bc.bz);
    font_draw(FONT_MONO, line, lx, yy + 18, 15, S_bc.bz[0] ? RGB(120, 220, 130) : g_theme.text_dim);
    status_line(line, sizeof(line), S_bc.initrd[0], "initramfs", S_bc.initrd);
    font_draw(FONT_MONO, line, lx, yy + 42, 15, S_bc.initrd[0] ? RGB(120, 220, 130) : g_theme.text_dim);
    font_draw(FONT_UI, S_bc.linux_ok ? "Linux payload ready to chainload"
                                     : "Place bzImage + initramfs.img in " PSA_DATA_DIR "/linux/",
              lx, yy + 68, 16, g_theme.text_dim);
    if (ui_button(700, x + cw - 210, yy + 92, 190, 42, "Boot Linux", S_bc.linux_ok)) { }
    if (ui_pressed(700)) {
        /* simulated handoff: pure UI state transfer, nothing reboots */
        if (S_bc.linux_ok) {
            desktop_set_os_target(OS_TARGET_LINUX);
            wm_open(WIN_BC_LINUX, "GNOME Terminal - Boot Camp", CHROME_LINUX,
                    560, 300, 880, 480, linux_shell_draw);
            desktop_toast("Chainloading Linux kernel... (simulated shell handoff)");
        } else {
            desktop_toast("Linux files not found");
        }
    }
    yy += 166;

    gfx_round_rect(x, yy, cw, 150, 12, RGB(38, 38, 44));
    icons_draw_glyph(ICON_WIN, x + 44, yy + 75, 44, RGB(120, 180, 255));
    status_line(line, sizeof(line), S_bc.win_ok, "Windows partition", S_bc.winpart);
    font_draw(FONT_MONO, line, lx, yy + 24, 15, S_bc.win_ok ? RGB(120, 220, 130) : g_theme.text_dim);
    font_draw(FONT_UI, S_bc.win_ok ? "NTFS/ exFAT volume detected - safe unmount recommended"
                                   : "No NTFS/exFAT volume mounted",
              lx, yy + 52, 16, g_theme.text_dim);
    if (ui_button(701, x + cw - 210, yy + 92, 190, 42, "Boot Windows", S_bc.win_ok)) { }
    if (ui_pressed(701)) {
        /* simulated handoff: pure UI state transfer, nothing reboots */
        if (S_bc.win_ok) {
            desktop_set_os_target(OS_TARGET_WINDOWS);
            wm_open(WIN_BC_WIN, "Command Prompt - Boot Camp", CHROME_WIN,
                    520, 260, 880, 480, win_shell_draw);
            desktop_toast("Rebooting into Windows Boot Manager... (simulated)");
        } else {
            desktop_toast("No Windows volume found");
        }
    }
    yy += 182;

    font_draw(FONT_BOLD, "Shell previews (chrome showcase)", x, yy, 21, g_theme.text_bright);
    yy += 40;
    if (ui_button(702, x, yy, 250, 46, "Open Windows shell", 0)) { }
    if (ui_button(703, x + 262, yy, 250, 46, "Open Linux shell", 0)) { }
    if (ui_button(704, x + 524, yy, 250, 46, "Use macOS shell", 0)) { }
    if (ui_pressed(702)) {
        desktop_set_os_target(OS_TARGET_WINDOWS);
        wm_open(WIN_BC_WIN, "Command Prompt - Boot Camp", CHROME_WIN, 520, 260, 880, 480, win_shell_draw);
    }
    if (ui_pressed(703)) {
        desktop_set_os_target(OS_TARGET_LINUX);
        wm_open(WIN_BC_LINUX, "GNOME Terminal - Boot Camp", CHROME_LINUX, 560, 300, 880, 480, linux_shell_draw);
    }
    if (ui_pressed(704)) {
        desktop_set_os_target(OS_TARGET_MACOS);
        desktop_toast("macOS Aqua Slate shell restored");
    }

    yy += 62;
    font_draw(FONT_UI, "Boot Camp reuses the real OS shell renderers and syncs the whole desktop",
              x, yy, 15, g_theme.text_dim);
    font_draw(FONT_UI, "background in a single frame. Foreign shells stay interactive: the Windows",
              x, yy + 20, 15, g_theme.text_dim);
    font_draw(FONT_UI, "taskbar runs a cascading Start menu, Linux boots through a CachyOS splash.",
              x, yy + 40, 15, g_theme.text_dim);
}

static void win_shell_draw(Win *w)
{
    static const char *lines[] = {
        "Microsoft Windows [Version 10.0.22631.3447]",
        "(c) Microsoft Corporation. All rights reserved.",
        "",
        "C:\\Users\\PS5> ver",
        "Microsoft Windows [Version 10.0.22631.3447]",
        "C:\\Users\\PS5> dir C:\\",
        " Volume in drive C is Windows-SSD",
        " Directory of C:\\",
        "",
        "10/17/2026  09:14 AM    <DIR>          Program Files",
        "10/17/2026  09:14 AM    <DIR>          Users",
        "10/17/2026  09:14 AM    <DIR>          Windows",
        "",
        "C:\\Users\\PS5>"
    };
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
        font_draw(FONT_MONO, lines[i], w->x + 16, w->y + 64 + i * 22, 16, RGB(204, 204, 204));
    if (w == wm_top() && (now_ms() / 500) % 2 == 0)
        gfx_rect(w->x + 16 + font_text_w(FONT_MONO, "C:\\Users\\PS5>", 16) + 4,
                 w->y + 64 + 13 * 22 + 2, 9, 17, RGB(204, 204, 204));
}

static void linux_shell_draw(Win *w)
{
    static const char *lines[] = {
        "ps5@sonoma:~$ uname -a",
        "Linux ps5 6.1.0-ps5 #1 SMP aarch64 GNU/Linux",
        "ps5@sonoma:~$ ls /mnt",
        "usb0  usb1  data",
        "ps5@sonoma:~$ neofetch",
        "       .-.        ps5@sonoma",
        "      (o o)       OS: Debian 12 (bookworm) aarch64",
        "      | O \\       Kernel: 6.1.0-ps5",
        "       \\   \\      Shell: bash 5.2.15",
        "        `~~~'     DE: GNOME 43 (PS5GreenApple chrome)",
        "ps5@sonoma:~$"
    };
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
        font_draw(FONT_MONO, lines[i], w->x + 16, w->y + 62 + i * 24, 16, RGB(214, 220, 229));
}
