#ifndef MENIOS_INCLUDE_KERNEL_BLOCK_CACHE_H
#define MENIOS_INCLUDE_KERNEL_BLOCK_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/block_device.h>

void block_cache_init(void);
void block_cache_shutdown(void);

bool block_cache_try_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count);
void block_cache_store(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
void block_cache_update(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
void block_cache_invalidate_device(block_device_t* device);
void block_cache_flush_device(block_device_t* device);

#ifdef __cplusplus
}
#endif

#endif
