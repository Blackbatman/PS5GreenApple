/* osk.h - on-screen keyboard (D-pad navigable) */
#ifndef OSK_H
#define OSK_H
#include "common.h"

#define OSK_FLAG_NONE         0
#define OSK_FLAG_ENTER_CLOSES 1   /* Enter confirms & closes (Terminal) */

void osk_open(char *buf, int cap, const char *title, int flags);
void osk_close(void);
int  osk_active(void);
int  osk_enter_pressed(void);   /* consumed once */
void osk_frame(void);           /* draw + handle input */
void osk_inject_char(char c);   /* for scripted tests */
void osk_inject_str(const char *s);

#endif
