#ifndef KERNEL_CONTEXT_SWITCH_H
#define KERNEL_CONTEXT_SWITCH_H

#include <stdint.h>

void context_switch(uint64_t* prev_rsp, uint64_t next_rsp);

#endif
