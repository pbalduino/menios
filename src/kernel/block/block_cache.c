#include <kernel/block_cache.h>

#include <kernel/condvar.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

#include <string.h>

#define BCACHE_MAX_BUFFERS        512u
#define BCACHE_HASH_BUCKETS       256u
#define BCACHE_MAX_BLOCK_BYTES    4096u
#define BCACHE_DIRTY_LIMIT        128u

typedef struct buffer_slot {
  buffer_head_t head;
  uint8_t       storage[BCACHE_MAX_BLOCK_BYTES];
} buffer_slot_t;

static bool          bcache_initialized = false;
static kmutex_t      bcache_lock;
static kcondvar_t    bcache_cv;
static buffer_slot_t bcache_pool[BCACHE_MAX_BUFFERS];
static buffer_head_t* bcache_hash[BCACHE_HASH_BUCKETS];
static buffer_head_t* bcache_lru_head = NULL;
static buffer_head_t* bcache_lru_tail = NULL;
static size_t        bcache_dirty_count = 0;

static inline uint32_t bcache_hash_key(block_device_t* device, uint64_t lba) {
  return (uint32_t)(((uintptr_t)device >> 4) ^ (uint32_t)lba) & (BCACHE_HASH_BUCKETS - 1u);
}

static void bcache_lru_detach(buffer_head_t* bh) {
  if(bh->lru_prev) {
    bh->lru_prev->lru_next = bh->lru_next;
  }
  if(bh->lru_next) {
    bh->lru_next->lru_prev = bh->lru_prev;
  }
  if(bcache_lru_head == bh) {
    bcache_lru_head = bh->lru_next;
  }
  if(bcache_lru_tail == bh) {
    bcache_lru_tail = bh->lru_prev;
  }
  bh->lru_prev = NULL;
  bh->lru_next = NULL;
}

static void bcache_lru_push_front(buffer_head_t* bh) {
  bh->lru_prev = NULL;
  bh->lru_next = bcache_lru_head;
  if(bcache_lru_head) {
    bcache_lru_head->lru_prev = bh;
  }
  bcache_lru_head = bh;
  if(bcache_lru_tail == NULL) {
    bcache_lru_tail = bh;
  }
}

static void bcache_hash_insert(buffer_head_t* bh) {
  uint32_t key = bcache_hash_key(bh->device, bh->lba);
  bh->hash_next = bcache_hash[key];
  bcache_hash[key] = bh;
}

static void bcache_hash_remove(buffer_head_t* bh) {
  uint32_t key = bcache_hash_key(bh->device, bh->lba);
  buffer_head_t** prev = &bcache_hash[key];
  while(*prev) {
    if(*prev == bh) {
      *prev = bh->hash_next;
      bh->hash_next = NULL;
      return;
    }
    prev = &(*prev)->hash_next;
  }
}

static buffer_head_t* bcache_hash_lookup(block_device_t* device, uint64_t lba) {
  uint32_t key = bcache_hash_key(device, lba);
  for(buffer_head_t* bh = bcache_hash[key]; bh != NULL; bh = bh->hash_next) {
    if(bh->valid && bh->device == device && bh->lba == lba) {
      return bh;
    }
  }
  return NULL;
}

static buffer_head_t* bcache_select_victim(void) {
  for(buffer_head_t* bh = bcache_lru_tail; bh != NULL; bh = bh->lru_prev) {
    if(bh->refcount == 0 && !bh->busy) {
      return bh;
    }
  }
  return NULL;
}

static bool bcache_flush_locked(buffer_head_t* bh) {
  if(bh == NULL || !bh->dirty || !bh->valid || bh->device == NULL) {
    return true;
  }
  block_device_t* dev = bh->device;
  if(dev->ops == NULL || dev->ops->write_blocks == NULL) {
    return false;
  }
  uint64_t lba = bh->lba;
  uint32_t block_size = bh->block_size;
  uint8_t local[BCACHE_MAX_BLOCK_BYTES];
  memcpy(local, bh->data, block_size);
  kmutex_unlock(&bcache_lock);
  bool ok = dev->ops->write_blocks(dev, lba, local, 1);
  kmutex_lock(&bcache_lock);
  if(ok && bh->dirty) {
    bh->dirty = false;
    if(bcache_dirty_count > 0) {
      bcache_dirty_count--;
    }
  }
  return ok;
}

static buffer_head_t* bcache_alloc_locked(void) {
  for(size_t i = 0; i < BCACHE_MAX_BUFFERS; i++) {
    buffer_head_t* bh = &bcache_pool[i].head;
    if(!bh->allocated) {
      bh->allocated = true;
      bh->data = bcache_pool[i].storage;
      return bh;
    }
  }
  return NULL;
}

static buffer_head_t* bcache_get_locked(block_device_t* device, uint64_t lba, bool* fresh_block) {
  while(true) {
    buffer_head_t* bh = bcache_hash_lookup(device, lba);
    if(bh != NULL) {
      if(bh->busy) {
        kcondvar_wait(&bcache_cv, &bcache_lock);
        continue;
      }
      bh->busy = true;
      bh->refcount++;
      bcache_lru_detach(bh);
      bcache_lru_push_front(bh);
      *fresh_block = false;
      return bh;
    }

    buffer_head_t* victim = bcache_select_victim();
    if(victim == NULL) {
      buffer_head_t* unused = bcache_alloc_locked();
      if(unused) {
        victim = unused;
        victim->refcount = 0;
        victim->dirty = false;
        victim->valid = false;
        victim->busy = false;
        victim->device = NULL;
        victim->lba = 0;
        victim->block_size = 0;
      }
    }

    if(victim == NULL) {
      kcondvar_wait(&bcache_cv, &bcache_lock);
      continue;
    }

    victim->busy = true;

    if(victim->valid && victim->dirty) {
      if(!bcache_flush_locked(victim)) {
        serial_printf("block_cache: failed to flush dirty buffer (device=%p lba=%llu)\n",
                      victim->device,
                      (unsigned long long)victim->lba);
      }
    }

    if(victim->valid) {
      bcache_hash_remove(victim);
    }

    victim->device = device;
    victim->lba = lba;
    victim->block_size = device->block_size;
    victim->valid = false;
    victim->dirty = false;
    victim->refcount = 1;
    bcache_hash_insert(victim);
    bcache_lru_detach(victim);
    bcache_lru_push_front(victim);
    *fresh_block = true;
    return victim;
  }
}

static void bcache_release_locked(buffer_head_t* bh) {
  if(bh == NULL) {
    return;
  }
  if(bh->refcount == 0) {
    return;
  }
  bh->refcount--;
  bh->busy = false;
  bcache_lru_detach(bh);
  bcache_lru_push_front(bh);
  kcondvar_broadcast(&bcache_cv);
}

void block_cache_initialize(void) {
  if(bcache_initialized) {
    return;
  }
  kmutex_init(&bcache_lock);
  kcondvar_init(&bcache_cv);
  memset(bcache_hash, 0, sizeof(bcache_hash));
  memset(bcache_pool, 0, sizeof(bcache_pool));
  bcache_lru_head = NULL;
  bcache_lru_tail = NULL;
  for(size_t i = 0; i < BCACHE_MAX_BUFFERS; i++) {
    buffer_head_t* bh = &bcache_pool[i].head;
    bh->data = bcache_pool[i].storage;
  }
  bcache_initialized = true;
}

void block_cache_shutdown(void) {
  if(!bcache_initialized) {
    return;
  }
  kmutex_lock(&bcache_lock);
  for(size_t i = 0; i < BCACHE_MAX_BUFFERS; i++) {
    buffer_head_t* bh = &bcache_pool[i].head;
    if(bh->dirty && bh->valid) {
      bcache_flush_locked(bh);
    }
    bh->allocated = false;
    bh->valid = false;
    bh->dirty = false;
    bh->busy = false;
    bh->refcount = 0;
    bh->device = NULL;
    bh->lba = 0;
    bh->block_size = 0;
    bh->hash_next = NULL;
    bh->lru_prev = NULL;
    bh->lru_next = NULL;
  }
  memset(bcache_hash, 0, sizeof(bcache_hash));
  bcache_lru_head = NULL;
  bcache_lru_tail = NULL;
  bcache_dirty_count = 0;
  bcache_initialized = false;
  kmutex_unlock(&bcache_lock);
}

buffer_head_t* bread(block_device_t* device, uint64_t lba) {
  if(device == NULL || device->block_size == 0 || device->block_size > BCACHE_MAX_BLOCK_BYTES) {
    return NULL;
  }
  if(device->ops == NULL || device->ops->read_blocks == NULL) {
    return NULL;
  }

  kmutex_lock(&bcache_lock);
  bool fresh = false;
  buffer_head_t* bh = bcache_get_locked(device, lba, &fresh);
  kmutex_unlock(&bcache_lock);
  if(bh == NULL) {
    return NULL;
  }

  if(fresh) {
    if(!device->ops->read_blocks(device, lba, bh->data, 1)) {
      kmutex_lock(&bcache_lock);
      bh->valid = false;
      bh->refcount = 0;
      bh->busy = false;
      bcache_hash_remove(bh);
      bcache_lru_detach(bh);
      bcache_lru_push_front(bh);
      kcondvar_broadcast(&bcache_cv);
      kmutex_unlock(&bcache_lock);
      return NULL;
    }
    kmutex_lock(&bcache_lock);
    bh->valid = true;
    bh->dirty = false;
    bh->block_size = device->block_size;
    kmutex_unlock(&bcache_lock);
  }

  return bh;
}

buffer_head_t* bget(block_device_t* device, uint64_t lba) {
  if(device == NULL || device->block_size == 0 || device->block_size > BCACHE_MAX_BLOCK_BYTES) {
    return NULL;
  }
  kmutex_lock(&bcache_lock);
  bool fresh = false;
  buffer_head_t* bh = bcache_get_locked(device, lba, &fresh);
  kmutex_unlock(&bcache_lock);
  if(bh == NULL) {
    return NULL;
  }
  if(fresh) {
    memset(bh->data, 0, device->block_size);
    kmutex_lock(&bcache_lock);
    bh->valid = true;
    kmutex_unlock(&bcache_lock);
  }
  return bh;
}

void bdirty(buffer_head_t* bh) {
  if(bh == NULL) {
    return;
  }
  kmutex_lock(&bcache_lock);
  if(!bh->dirty) {
    bh->dirty = true;
    bcache_dirty_count++;
  }
  if(bcache_dirty_count > BCACHE_DIRTY_LIMIT) {
    for(buffer_head_t* tail = bcache_lru_tail; tail != NULL; tail = tail->lru_prev) {
      if(tail->dirty && tail->refcount == 0 && !tail->busy) {
        bcache_flush_locked(tail);
        break;
      }
    }
  }
  kmutex_unlock(&bcache_lock);
}

bool bwrite(buffer_head_t* bh) {
  if(bh == NULL || bh->device == NULL || bh->device->ops == NULL || bh->device->ops->write_blocks == NULL) {
    return false;
  }
  uint8_t local[BCACHE_MAX_BLOCK_BYTES];
  block_device_t* dev;
  uint64_t lba;
  uint32_t block_size;

  kmutex_lock(&bcache_lock);
  dev = bh->device;
  lba = bh->lba;
  block_size = bh->block_size;
  memcpy(local, bh->data, block_size);
  kmutex_unlock(&bcache_lock);

  if(!dev->ops->write_blocks(dev, lba, local, 1)) {
    return false;
  }

  kmutex_lock(&bcache_lock);
  if(bh->dirty) {
    bh->dirty = false;
    if(bcache_dirty_count > 0) {
      bcache_dirty_count--;
    }
  }
  kmutex_unlock(&bcache_lock);
  return true;
}

void brelse(buffer_head_t* bh) {
  if(bh == NULL) {
    return;
  }
  kmutex_lock(&bcache_lock);
  bcache_release_locked(bh);
  kmutex_unlock(&bcache_lock);
}

void block_cache_flush_device(block_device_t* device) {
  if(device == NULL) {
    return;
  }
  kmutex_lock(&bcache_lock);
  for(size_t i = 0; i < BCACHE_MAX_BUFFERS; i++) {
    buffer_head_t* bh = &bcache_pool[i].head;
    if(bh->device == device && bh->dirty && bh->valid) {
      if(!bcache_flush_locked(bh)) {
        serial_printf("block_cache: flush_device failed (device=%p lba=%llu)\n",
                      device,
                      (unsigned long long)bh->lba);
      }
    }
  }
  kmutex_unlock(&bcache_lock);
}

void block_cache_invalidate_device(block_device_t* device) {
  if(device == NULL) {
    return;
  }
  kmutex_lock(&bcache_lock);
  for(size_t i = 0; i < BCACHE_MAX_BUFFERS; i++) {
    buffer_head_t* bh = &bcache_pool[i].head;
    if(bh->device == device) {
      if(bh->dirty && bh->valid) {
        if(!bcache_flush_locked(bh)) {
          serial_printf("block_cache: invalidate flush failed (device=%p lba=%llu)\n",
                        device,
                        (unsigned long long)bh->lba);
        }
      }
      if(bh->valid) {
        bcache_hash_remove(bh);
      }
      bh->valid = false;
      bh->dirty = false;
      bh->device = NULL;
      bh->lba = 0;
      bh->block_size = 0;
      bcache_lru_detach(bh);
      bcache_lru_push_front(bh);
    }
  }
  kcondvar_broadcast(&bcache_cv);
  kmutex_unlock(&bcache_lock);
}
