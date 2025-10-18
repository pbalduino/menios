#include <kernel/block_device.h>
#include <kernel/block_cache.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

#include <string.h>

static kmutex_t block_device_lock;
static block_device_t* block_device_head = NULL;
static bool block_device_initialized = false;

typedef struct block_io_request_t {
  uint64_t lba;
  size_t block_count;
  void* buffer;
  bool write;
  bool processed;
  bool success;
  struct block_io_request_t* next;
} block_io_request_t;

#if 0
static void block_queue_insert(block_device_t* device, block_io_request_t* request);
static block_io_request_t* block_queue_peek_next(block_device_t* device);
static block_io_request_t* block_queue_pop_next(block_device_t* device);
static void block_queue_insert_sorted(block_io_request_t** head,
                                      block_io_request_t* request,
                                      bool ascending);
static bool block_device_submit(block_device_t* device,
                                uint64_t lba,
                                void* buffer,
                                size_t block_count,
                                bool write);
#endif

void block_device_system_init(void) {
  kmutex_init(&block_device_lock);
  block_cache_init();
  block_device_head = NULL;
  block_device_initialized = true;
}

#define BLOCK_DEVICE_READAHEAD_WINDOW 4u

static void block_device_readahead(block_device_t* device, uint64_t start_lba, uint32_t window) {
  if(device == NULL || device->ops == NULL || device->ops->read_blocks == NULL) {
    return;
  }
  if(start_lba >= device->block_count) {
    return;
  }
  uint64_t remaining = device->block_count - start_lba;
  uint32_t count = window < remaining ? window : (uint32_t)remaining;
  for(uint32_t i = 0; i < count; i++) {
    buffer_head_t* bh = bread(device, start_lba + i);
    if(bh == NULL) {
      break;
    }
    brelse(bh);
  }
}

static bool block_device_name_exists(const char* name) {
  for(block_device_t* node = block_device_head; node != NULL; node = node->next) {
    if(strncmp(node->name, name, sizeof(node->name)) == 0) {
      return true;
    }
  }
  return false;
}

bool block_device_register(block_device_t* device) {
  if(!block_device_initialized || device == NULL || device->ops == NULL) {
    return false;
  }

  if(device->block_size == 0) {
    return false;
  }

  kmutex_lock(&block_device_lock);
  if(block_device_name_exists(device->name)) {
    kmutex_unlock(&block_device_lock);
    serial_printf("block_device_register: device '%s' already registered\n", device->name);
    return false;
  }

  kmutex_init(&device->queue_lock);
  kcondvar_init(&device->queue_cv);
  device->queue_up = NULL;
  device->queue_down = NULL;
  device->queue_direction_up = true;
  device->queue_busy = false;
  device->queue_last_lba = 0;
  device->readahead_last_lba = UINT64_MAX;
  device->readahead_last_count = 0;

  device->next = block_device_head;
  block_device_head = device;
  kmutex_unlock(&block_device_lock);

  serial_printf("block_device_register: registered block device '%s' (%lu blocks of %u bytes)\n",
                device->name,
                (unsigned long)device->block_count,
                device->block_size);
  return true;
}

void block_device_unregister(block_device_t* device) {
  if(!block_device_initialized || device == NULL) {
    return;
  }

  block_cache_invalidate_device(device);

  kmutex_lock(&block_device_lock);
  block_device_t** prev = &block_device_head;
  while(*prev) {
    if(*prev == device) {
      *prev = device->next;
      device->next = NULL;
      break;
    }
    prev = &(*prev)->next;
  }
  kmutex_unlock(&block_device_lock);
}

block_device_t* block_device_lookup(const char* name) {
  if(!block_device_initialized || name == NULL) {
    return NULL;
  }

  kmutex_lock(&block_device_lock);
  block_device_t* node = block_device_head;
  while(node) {
    if(strncmp(node->name, name, sizeof(node->name)) == 0) {
      kmutex_unlock(&block_device_lock);
      return node;
    }
    node = node->next;
  }
  kmutex_unlock(&block_device_lock);
  return NULL;
}

block_device_t* block_device_first(void) {
  if(!block_device_initialized) {
    return NULL;
  }
  return block_device_head;
}

block_device_t* block_device_next(block_device_t* current) {
  if(current == NULL) {
    return NULL;
  }
  return current->next;
}

#if 0
static void block_queue_insert_sorted(block_io_request_t** head,
                                      block_io_request_t* request,
                                      bool ascending) {
  if(*head == NULL ||
     (ascending ? (request->lba < (*head)->lba) : (request->lba > (*head)->lba))) {
    request->next = *head;
    *head = request;
    return;
  }

  block_io_request_t* node = *head;
  while(node->next &&
        (ascending ? (node->next->lba <= request->lba) : (node->next->lba >= request->lba))) {
    node = node->next;
  }
  request->next = node->next;
  node->next = request;
}

static void block_queue_insert(block_device_t* device, block_io_request_t* request) {
  request->next = NULL;

  if(device->queue_up == NULL && device->queue_down == NULL) {
    block_queue_insert_sorted(&device->queue_up, request, true);
    return;
  }

  if(request->lba >= device->queue_last_lba) {
    block_queue_insert_sorted(&device->queue_up, request, true);
  } else {
    block_queue_insert_sorted(&device->queue_down, request, false);
  }
}

static block_io_request_t* block_queue_peek_next(block_device_t* device) {
  if(device->queue_direction_up) {
    if(device->queue_up) {
      return device->queue_up;
    }
    if(device->queue_down) {
      return device->queue_down;
    }
  } else {
    if(device->queue_down) {
      return device->queue_down;
    }
    if(device->queue_up) {
      return device->queue_up;
    }
  }
  return NULL;
}

static block_io_request_t* block_queue_pop_next(block_device_t* device) {
  block_io_request_t* request = NULL;

  if(device->queue_direction_up) {
    if(device->queue_up) {
      request = device->queue_up;
      device->queue_up = request->next;
    } else if(device->queue_down) {
      device->queue_direction_up = false;
      request = device->queue_down;
      device->queue_down = request->next;
    }
  } else {
    if(device->queue_down) {
      request = device->queue_down;
      device->queue_down = request->next;
    } else if(device->queue_up) {
      device->queue_direction_up = true;
      request = device->queue_up;
      device->queue_up = request->next;
    }
  }

  if(request) {
    request->next = NULL;
  }
  return request;
}
#endif

static bool block_device_validate(block_device_t* device) {
  return device && device->ops && device->ops->read_blocks && device->block_size != 0;
}

#if 0
static bool block_device_submit(block_device_t* device,
                                uint64_t lba,
                                void* buffer,
                                size_t block_count,
                                bool write) {
  block_io_request_t* request = kmalloc(sizeof(block_io_request_t));
  if(request == NULL) {
    return false;
  }

  request->lba = lba;
  request->block_count = block_count;
  request->buffer = buffer;
  request->write = write;
  request->processed = false;
  request->success = false;
  request->next = NULL;

  kmutex_lock(&device->queue_lock);
  block_queue_insert(device, request);

  bool executor = false;

  while(!request->processed) {
    if(!executor && !device->queue_busy) {
      block_io_request_t* next = block_queue_peek_next(device);
      if(next == request) {
        block_io_request_t* popped = block_queue_pop_next(device);
        (void)popped; /* popped must equal request */
        device->queue_busy = true;
        executor = true;
        break;
      }
    }
    kcondvar_wait(&device->queue_cv, &device->queue_lock);
  }

  kmutex_unlock(&device->queue_lock);

  bool success = false;

  if(executor) {
    if(write) {
      success = device->ops->write_blocks(device, lba, request->buffer, block_count);
    } else {
      success = device->ops->read_blocks(device, lba, request->buffer, block_count);
    }

    kmutex_lock(&device->queue_lock);
    device->queue_busy = false;
    device->queue_last_lba = lba;
    request->success = success;
    request->processed = true;
    if(device->queue_up == NULL && device->queue_down == NULL) {
      device->queue_direction_up = true;
    }
    kcondvar_broadcast(&device->queue_cv);
    kmutex_unlock(&device->queue_lock);
  } else {
    kmutex_lock(&device->queue_lock);
    while(!request->processed) {
      kcondvar_wait(&device->queue_cv, &device->queue_lock);
    }
    success = request->success;
    kmutex_unlock(&device->queue_lock);
  }

  kfree(request);
  return success;
}
#endif

bool block_device_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  if(!block_device_validate(device) || buffer == NULL || block_count == 0) {
    return false;
  }
  if(lba + block_count > device->block_count) {
    return false;
  }
  bool sequential = false;
  if(device->readahead_last_count > 0 && device->readahead_last_lba != UINT64_MAX) {
    uint64_t expected = device->readahead_last_lba + device->readahead_last_count;
    if(lba == expected) {
      sequential = true;
    }
  }

  uint8_t* out = (uint8_t*)buffer;
  size_t block_size = device->block_size;
  for(size_t i = 0; i < block_count; i++) {
    buffer_head_t* bh = bread(device, lba + i);
    if(bh == NULL) {
      return false;
    }
    memcpy(out + i * block_size, bh->data, block_size);
    brelse(bh);
  }

  device->readahead_last_lba = lba;
  device->readahead_last_count = (uint32_t)block_count;

  uint64_t next_lba = lba + block_count;
  if(sequential || block_count > 1) {
    block_device_readahead(device, next_lba, BLOCK_DEVICE_READAHEAD_WINDOW);
  }

  return true;
}

bool block_device_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  if(device == NULL || buffer == NULL || block_count == 0) {
    return false;
  }
  if(device->ops == NULL || device->ops->write_blocks == NULL) {
    return false;
  }
  if(lba + block_count > device->block_count) {
    return false;
  }
  const uint8_t* src = (const uint8_t*)buffer;
  size_t block_size = device->block_size;
  for(size_t i = 0; i < block_count; i++) {
    buffer_head_t* bh = bget(device, lba + i);
    if(bh == NULL) {
      return false;
    }
    memcpy(bh->data, src + i * block_size, block_size);
    bh->block_size = block_size;
    bh->valid = true;
    bdirty(bh);
    brelse(bh);
  }
  return true;
}

bool block_device_flush(block_device_t* device) {
  if(device == NULL || device->ops == NULL || device->ops->flush == NULL) {
    return true;
  }
  block_cache_flush_device(device);
  return device->ops->flush(device);
}
