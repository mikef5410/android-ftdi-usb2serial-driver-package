#ifndef _TC_FDT_H
#define _TC_FDT_H

#include <sys/types.h>

extern void tc_fdt_init(void);
extern void tc_fdt_set(u_int16_t count);
extern int tc_fdt_get_next_slot(int reference_time, int slotlen);

#endif
