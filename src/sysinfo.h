/* sysinfo.h - hardware/OS info for About + System Report */
#ifndef SYSINFO_H
#define SYSINFO_H
#include "common.h"

typedef struct SysInfo {
    char model[64];
    char fw[32];
    char kernel[96];
    char cpu[96];
    char cpu_cores[16];
    char mem_total[32];
    char loader[64];
    char display[32];
    char renderer[48];
    long disk_total_mb, disk_free_mb;
    int  fw_detected;      /* 1 = read from real sysctl/proc */
    int  mem_detected;
} SysInfo;

void sysinfo_collect(SysInfo *si);
void sysinfo_mark_boot(void);
const char *sysinfo_uptime_str(char *buf, int n);

#endif
