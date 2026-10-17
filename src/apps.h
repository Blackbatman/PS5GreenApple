/* apps.h - dock registry + app entry points */
#ifndef APPS_H
#define APPS_H
#include "common.h"
#include "icons.h"

enum {
    APP_FINDER = 0, APP_TERMINAL, APP_TEXTEDIT, APP_ABOUT,
    APP_SETTINGS, APP_BOOTCAMP, APP_COUNT
};
#define APP_TRASH 100

typedef struct { int app; const char *name; IconKind icon; } DockEntry;
extern const DockEntry g_dock[APP_COUNT + 1];   /* + Trash */

void app_finder_open(void);
void app_terminal_open(void);
void app_textedit_open(void);
void app_about_open(void);
void app_settings_open(void);
void app_bootcamp_open(void);
void app_textedit_save(void);      /* square shortcut */
void app_textedit_open_path(const char *path);
void app_terminal_run(const char *cmd); /* programmatic exec */

#endif
