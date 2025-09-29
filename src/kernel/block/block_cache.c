#include <kernel/block_cache.h>

#include <string.h>

#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

#define BLOCK_CACHE_MAX_ENTRIES     256u
#define BLOCK_CACHE_MAX_BLOCK_BYTES 4096u

typedef struct block_cache_entry_t {
  block_device_t*            device;
  uint64_t                   lba;
  size_t                     block_size;
  struct block_cache_entry_t* prev;
  struct block_cache_entry_t* next;
  bool                       valid;
  uint8_t                    data[BLOCK_CACHE_MAX_BLOCK_BYTES];
} block_cache_entry_t;

static bool                 block_cache_initialized = false;
static kmutex_t             block_cache_lock;
static block_cache_entry_t  block_cache_entries[BLOCK_CACHE_MAX_ENTRIES];
static block_cache_entry_t* block_cache_lru_head = NULL;
static block_cache_entry_t* block_cache_lru_tail = NULL;

static bool block_cache_device_supported(block_device_t* device) {
  if(device == NULL) {
    return false;
  }
  if(device->block_size == 0 || device->block_size > BLOCK_CACHE_MAX_BLOCK_BYTES) {
    return false;
  }
  return true;
}

static void block_cache_lru_detach(block_cache_entry_t* entry) {
  if(entry->prev) {
    entry->prev->next = entry->next;
  }
  if(entry->next) {
    entry->next->prev = entry->prev;
  }
  if(block_cache_lru_head == entry) {
    block_cache_lru_head = entry->next;
  }
  if(block_cache_lru_tail == entry) {
    block_cache_lru_tail = entry->prev;
  }
  entry->prev = NULL;
  entry->next = NULL;
}

static void block_cache_lru_push_front(block_cache_entry_t* entry) {
  entry->prev = NULL;
  entry->next = block_cache_lru_head;
  if(block_cache_lru_head) {
    block_cache_lru_head->prev = entry;
  }
  block_cache_lru_head = entry;
  if(block_cache_lru_tail == NULL) {
    block_cache_lru_tail = entry;
  }
}

static block_cache_entry_t* block_cache_find(block_device_t* device, uint64_t lba) {
  for(block_cache_entry_t* entry = block_cache_lru_head; entry != NULL; entry = entry->next) {
    if(entry->valid && entry->device == device && entry->lba == lba) {
      return entry;
    }
  }
  return NULL;
}

static block_cache_entry_t* block_cache_acquire_entry(block_device_t* device, uint64_t lba, size_t block_size) {
  block_cache_entry_t* entry = block_cache_find(device, lba);
  if(entry) {
    block_cache_lru_detach(entry);
    block_cache_lru_push_front(entry);
    return entry;
  }

  for(size_t i = 0; i < BLOCK_CACHE_MAX_ENTRIES; i++) {
    if(!block_cache_entries[i].valid) {
      entry = &block_cache_entries[i];
      block_cache_lru_detach(entry);
      block_cache_lru_push_front(entry);
      entry->device = device;
      entry->lba = lba;
      entry->block_size = block_size;
      entry->valid = true;
      return entry;
    }
  }

  entry = block_cache_lru_tail;
  if(entry == NULL) {
    return NULL;
  }

  block_cache_lru_detach(entry);
  entry->device = device;
  entry->lba = lba;
  entry->block_size = block_size;
  entry->valid = true;
  block_cache_lru_push_front(entry);
  return entry;
}

static void block_cache_invalidate_entry(block_cache_entry_t* entry) {
  if(entry == NULL) {
    return;
  }
  block_cache_lru_detach(entry);
  entry->valid = false;
  entry->device = NULL;
  entry->lba = 0;
  entry->block_size = 0;
}

void block_cache_init(void) {
  if(block_cache_initialized) {
    return;
  }
  kmutex_init(&block_cache_lock);
  memset(block_cache_entries, 0, sizeof(block_cache_entries));
  block_cache_lru_head = NULL;
  block_cache_lru_tail = NULL;
  block_cache_initialized = true;
}

void block_cache_shutdown(void) {
  if(!block_cache_initialized) {
    return;
  }
  kmutex_lock(&block_cache_lock);
  for(size_t i = 0; i < BLOCK_CACHE_MAX_ENTRIES; i++) {
    block_cache_invalidate_entry(&block_cache_entries[i]);
  }
  block_cache_lru_head = NULL;
  block_cache_lru_tail = NULL;
  kmutex_unlock(&block_cache_lock);
  block_cache_initialized = false;
}

bool block_cache_try_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  if(!block_cache_initialized || !block_cache_device_supported(device) || buffer == NULL || block_count == 0) {
    return false;
  }

  uint8_t* out = (uint8_t*)buffer;
  kmutex_lock(&block_cache_lock);
  for(size_t idx = 0; idx < block_count; idx++) {
    block_cache_entry_t* entry = block_cache_find(device, lba + idx);
    if(entry == NULL || !entry->valid || entry->block_size != device->block_size) {
      kmutex_unlock(&block_cache_lock);
      return false;
    }
    block_cache_lru_detach(entry);
    block_cache_lru_push_front(entry);
    memcpy(out + idx * device->block_size, entry->data, device->block_size);
  }
  kmutex_unlock(&block_cache_lock);
  return true;
}

void block_cache_store(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  if(!block_cache_initialized || !block_cache_device_supported(device) || buffer == NULL || block_count == 0) {
    return;
  }

  const uint8_t* src = (const uint8_t*)buffer;
  kmutex_lock(&block_cache_lock);
  for(size_t idx = 0; idx < block_count; idx++) {
    block_cache_entry_t* entry = block_cache_acquire_entry(device, lba + idx, device->block_size);
    if(entry == NULL) {
      continue;
    }
    memcpy(entry->data, src + idx * device->block_size, device->block_size);
  }
  kmutex_unlock(&block_cache_lock);
}

void block_cache_update(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  if(!block_cache_initialized || !block_cache_device_supported(device) || buffer == NULL || block_count == 0) {
    return;
  }

  const uint8_t* src = (const uint8_t*)buffer;
  kmutex_lock(&block_cache_lock);
  for(size_t idx = 0; idx < block_count; idx++) {
    block_cache_entry_t* entry = block_cache_acquire_entry(device, lba + idx, device->block_size);
    if(entry == NULL) {
      continue;
    }
    memcpy(entry->data, src + idx * device->block_size, device->block_size);
  }
  kmutex_unlock(&block_cache_lock);
}

void block_cache_invalidate_device(block_device_t* device) {
  if(!block_cache_initialized || device == NULL) {
    return;
  }
  kmutex_lock(&block_cache_lock);
  for(size_t i = 0; i < BLOCK_CACHE_MAX_ENTRIES; i++) {
    if(block_cache_entries[i].valid && block_cache_entries[i].device == device) {
      block_cache_invalidate_entry(&block_cache_entries[i]);
    }
  }
  kmutex_unlock(&block_cache_lock);
}

void block_cache_flush_device(block_device_t* device) {
  (void)device;
  // Write-through cache: nothing to flush for now.
}
