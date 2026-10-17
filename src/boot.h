/* boot.h - Apple-style boot animation */
#ifndef BOOT_H
#define BOOT_H
#include "common.h"

void boot_reset(void);
int  boot_active(void);          /* 1 while playing */
int  boot_frame(void);           /* draws; returns 1 when finished */
void boot_skip(void);

#endif
