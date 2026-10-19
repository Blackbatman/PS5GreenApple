/* font.h - stb_truetype text rendering over .incbin fonts */
#ifndef FONT_H
#define FONT_H
#include "common.h"

enum { FONT_UI = 0, FONT_BOLD = 1, FONT_MONO = 2, FONT_COUNT = 3 };

int  font_init(void);
void font_shutdown(void);
int  font_draw(int id, const char *s, int x, int y, float size, uint32_t c); /* y = top; returns advance */

/* canonical text-layer entry point: UI font at default size.
 * All modules must draw text through this or font_draw(); never raw printers. */
int  font_draw_string(int x, int y, const char *str, uint32_t color);

int  font_text_w(int id, const char *s, float size);
int  font_line_h(int id, float size);
int  font_utf8_next(const char *s, unsigned *out_cp);
#define FONT_UI_SIZE 20.0f   /* default UI text size */

#endif
