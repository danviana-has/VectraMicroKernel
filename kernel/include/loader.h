#ifndef VECTRA_LOADER_H
#define VECTRA_LOADER_H

#include "types.h"
#include "task.h"

#define USER_CODE_VIRT_ADDR  0x40000000
#define USER_STACK_VIRT_ADDR 0x40100000

int32_t bin_load_and_run(const char* app_name, bool blocking);
const char* loader_get_last_cmdline(void);

#endif /* VECTRA_LOADER_H */
