#ifndef MENIOS_INCLUDE_KERNEL_BLOCK_CACHE_H
#define MENIOS_INCLUDE_KERNEL_BLOCK_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/block_device.h>

typedef struct buffer_head {
  block_device_t*      device;
  uint64_t             lba;
  uint32_t             block_size;
  uint8_t*             data;
  bool                 valid;
  bool                 dirty;
  bool                 busy;
  bool                 allocated;
  uint32_t             refcount;
  struct buffer_head*  hash_next;
  struct buffer_head*  lru_prev;
  struct buffer_head*  lru_next;
} buffer_head_t;

void block_cache_init(void);
void block_cache_shutdown(void);

buffer_head_t* bread(block_device_t* device, uint64_t lba);
buffer_head_t* bget(block_device_t* device, uint64_t lba);
void bdirty(buffer_head_t* bh);
bool bwrite(buffer_head_t* bh);
void brelse(buffer_head_t* bh);
void block_cache_invalidate_device(block_device_t* device);
void block_cache_flush_device(block_device_t* device);

#ifdef __cplusplus
}
#endif

#endif
