#pragma once

#ifdef MENIOS_HOST_TEST

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct block_header block_header_t;
typedef struct arena_header arena_header_t;

void __menios_allocator_reset(void);
int __menios_allocator_grow_heap_for_test(size_t size);

block_header_t* __menios_buddy_debug_pop(uint32_t order);
void __menios_buddy_debug_push(block_header_t* block);
block_header_t* __menios_buddy_debug_split(block_header_t* block, uint32_t target_order);
block_header_t* __menios_buddy_debug_coalesce(block_header_t* block);
uint32_t __menios_buddy_debug_order(const block_header_t* block);
uintptr_t __menios_buddy_debug_offset(const block_header_t* block);
arena_header_t* __menios_buddy_debug_arena(const block_header_t* block);
size_t __menios_buddy_debug_freelist_length(uint32_t order);
void __menios_buddy_debug_poison_after_remove(bool enable);

#endif /* MENIOS_HOST_TEST */
