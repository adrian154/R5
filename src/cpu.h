#ifndef __CORE_H
#define __CORE_H

#include <stdint.h>

#define PL_USER         0x0
#define PL_SUPERVISOR   0x1
#define PL_MACHINE      0x3

typedef struct {
    uint64_t regs[32];
    uint64_t pc;
} CPU;

#endif