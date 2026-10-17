/* settings.h - persisted user settings (PSA_DATA_DIR/settings.ini) */
#ifndef SETTINGS_H
#define SETTINGS_H
#include "common.h"

typedef struct Settings {
    int wallpaper;   /* 0=Sonoma Aurora 1=Deep Ocean 2=Sunset Ridge */
    int accent;      /* index into theme accent table */
    int clock24;     /* 24h clock */
    int seconds;     /* show seconds */
    int fps;         /* fps overlay */
} Settings;

extern Settings g_settings;
void settings_load(void);
void settings_save(void);
const char *wallpaper_name(int idx);
int  wallpaper_count(void);

#endif
