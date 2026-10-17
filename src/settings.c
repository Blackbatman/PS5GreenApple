/* settings.c - tiny key=value ini persistence */
#include "settings.h"

Settings g_settings = { 0, 0, 0, 0, 0 };

static void parse_line(char *line)
{
    char *eq = strchr(line, '=');
    if (!eq) return;
    *eq = 0;
    char *k = line, *v = eq + 1;
    int val = atoi(v);
    if      (!strcmp(k, "wallpaper")) g_settings.wallpaper = val;
    else if (!strcmp(k, "accent"))    g_settings.accent = val;
    else if (!strcmp(k, "clock24"))   g_settings.clock24 = val;
    else if (!strcmp(k, "seconds"))   g_settings.seconds = val;
    else if (!strcmp(k, "fps"))       g_settings.fps = val;
}

void settings_load(void)
{
    char path[256];
    path_join(path, sizeof(path), PSA_DATA_DIR, "settings.ini");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        for (char *p = line; *p; p++)
            if (*p == '\n' || *p == '\r') *p = 0;
        if (line[0] == '#' || line[0] == 0) continue;
        parse_line(line);
    }
    fclose(f);
    if (g_settings.wallpaper < 0 || g_settings.wallpaper > 2) g_settings.wallpaper = 0;
}

void settings_save(void)
{
    ensure_dir(PSA_DATA_DIR);
    char path[256];
    path_join(path, sizeof(path), PSA_DATA_DIR, "settings.ini");
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# PS5GreenApple settings\n");
    fprintf(f, "wallpaper=%d\n", g_settings.wallpaper);
    fprintf(f, "accent=%d\n", g_settings.accent);
    fprintf(f, "clock24=%d\n", g_settings.clock24);
    fprintf(f, "seconds=%d\n", g_settings.seconds);
    fprintf(f, "fps=%d\n", g_settings.fps);
    fclose(f);
}

const char *wallpaper_name(int idx)
{
    static const char *names[] = { "Sonoma Aurora", "Deep Ocean", "Sunset Ridge" };
    if (idx < 0 || idx > 2) return names[0];
    return names[idx];
}

int wallpaper_count(void) { return 3; }
