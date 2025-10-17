#ifndef MENIOS_INCLUDE_KERNEL_BLOCK_DEVICE_H
#define MENIOS_INCLUDE_KERNEL_BLOCK_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/condvar.h>
#include <kernel/mutex.h>

struct block_device_t;
struct block_io_request_t;
typedef struct block_device_t block_device_t;

typedef struct block_device_ops_t {
  bool (*read_blocks)(block_device_t* device, uint64_t lba, void* buffer, size_t block_count);
  bool (*write_blocks)(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
  bool (*flush)(block_device_t* device);
} block_device_ops_t;

struct block_device_t {
  char                 name[32];
  uint32_t             block_size;
  uint64_t             block_count;
  const block_device_ops_t* ops;
  void*                driver_ctx;
  block_device_t*      next;
  kmutex_t             queue_lock;
  kcondvar_t           queue_cv;
  struct block_io_request_t* queue_up;
  struct block_io_request_t* queue_down;
  bool                 queue_direction_up;
  bool                 queue_busy;
  uint64_t             queue_last_lba;
  uint64_t             readahead_last_lba;
  uint32_t             readahead_last_count;
};

typedef bool (*block_device_iter_t)(block_device_t* device, void* context);

void block_device_system_init(void);
bool block_device_register(block_device_t* device);
void block_device_unregister(block_device_t* device);
block_device_t* block_device_lookup(const char* name);
block_device_t* block_device_first(void);
block_device_t* block_device_next(block_device_t* current);
bool block_device_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count);
bool block_device_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
bool block_device_flush(block_device_t* device);

#ifdef __cplusplus
}
#endif

#endif
