#include <kernel/shm.h>

#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <stdbool.h>
#include <string.h>

#ifndef SHM_MAX_REGIONS
#define SHM_MAX_REGIONS 1024
#endif

struct shm_region {
  int          id;
  shm_key_t    key;
  size_t       size_bytes;
  size_t       page_count;
  uint16_t     mode;
  bool         marked_for_removal;
  size_t       refcount;
  size_t       attachment_count;
  phys_addr_t* pages;
  kmutex_t     lock;
  struct shm_region* next;
};

static struct {
  kmutex_t lock;
  shm_region_t* head;
  int next_id;
  size_t region_count;
} shm_manager;

static shm_page_alloc_fn alloc_pages_fn = pmm_alloc_pages;
static shm_page_free_fn free_pages_fn = pmm_free_pages;

static size_t align_page_count(size_t size) {
  if(size == 0) {
    return 0;
  }
  size_t pages = size / PAGE_SIZE;
  if((size % PAGE_SIZE) != 0) {
    pages++;
  }
  return pages;
}

static void zero_physical_page(phys_addr_t phys) {
  void* ptr = (void*)physical_to_virtual(phys);
  if(ptr != NULL) {
    memset(ptr, 0, PAGE_SIZE);
  }
}

static void shm_region_destroy(shm_region_t* region) {
  if(region == NULL) {
    return;
  }

  if(region->pages != NULL) {
    for(size_t i = 0; i < region->page_count; ++i) {
      if(region->pages[i] != 0) {
        if(free_pages_fn) {
          free_pages_fn(phys_frame_from_addr(region->pages[i]), 1);
        }
      }
    }
    kfree(region->pages);
  }

  kfree(region);
}

static int shm_manager_allocate_id_locked(void) {
  int attempts = SHM_MAX_REGIONS;
  while(attempts-- > 0) {
    int candidate = shm_manager.next_id++;
    if(shm_manager.next_id <= 0) {
      shm_manager.next_id = 1;
    }

    bool in_use = false;
    for(shm_region_t* it = shm_manager.head; it != NULL; it = it->next) {
      if(it->id == candidate) {
        in_use = true;
        break;
      }
    }

    if(!in_use) {
      return candidate;
    }
  }
  return -1;
}

void shm_manager_initialize(void) {
  kmutex_init(&shm_manager.lock);
  shm_manager.head = NULL;
  shm_manager.next_id = 1;
  shm_manager.region_count = 0;
}

void shm_set_allocator(shm_page_alloc_fn alloc_fn,
                       shm_page_free_fn free_fn) {
  alloc_pages_fn = alloc_fn ? alloc_fn : pmm_alloc_pages;
  free_pages_fn = free_fn ? free_fn : pmm_free_pages;
}

static shm_region_t* shm_manager_insert(shm_region_t* region) {
  shm_region_t* inserted = NULL;
  kmutex_lock(&shm_manager.lock);
  if(shm_manager.region_count >= SHM_MAX_REGIONS) {
    kmutex_unlock(&shm_manager.lock);
    return NULL;
  }
  int id = shm_manager_allocate_id_locked();
  if(id < 0) {
    kmutex_unlock(&shm_manager.lock);
    return NULL;
  }

  region->id = id;
  region->next = shm_manager.head;
  shm_manager.head = region;
  shm_manager.region_count++;
  inserted = region;
  kmutex_unlock(&shm_manager.lock);
  return inserted;
}

static void shm_manager_remove(shm_region_t* region) {
  kmutex_lock(&shm_manager.lock);
  shm_region_t* prev = NULL;
  shm_region_t* node = shm_manager.head;
  while(node != NULL) {
    if(node == region) {
      if(prev == NULL) {
        shm_manager.head = node->next;
      } else {
        prev->next = node->next;
      }
      if(shm_manager.region_count > 0) {
        shm_manager.region_count--;
      }
      break;
    }
    prev = node;
    node = node->next;
  }
  kmutex_unlock(&shm_manager.lock);
}

static bool shm_region_allocate_pages(shm_region_t* region) {
  region->pages = kmalloc(sizeof(phys_addr_t) * region->page_count);
  if(region->pages == NULL) {
    return false;
  }

  for(size_t i = 0; i < region->page_count; ++i) {
    phys_frame_t frame = alloc_pages_fn ? alloc_pages_fn(1) : phys_frame_invalid();
    if(!phys_frame_is_valid(frame)) {
      for(size_t j = 0; j < i; ++j) {
        if(free_pages_fn) {
          free_pages_fn(phys_frame_from_addr(region->pages[j]), 1);
        }
      }
      kfree(region->pages);
      region->pages = NULL;
      return false;
    }
    phys_addr_t phys = phys_frame_to_addr(frame);
    zero_physical_page(phys);
    region->pages[i] = phys;
  }

  return true;
}

shm_region_t* shm_region_create(shm_key_t key,
                                size_t size,
                                uint16_t mode,
                                int* id_out,
                                shm_region_create_status_t* status_out) {
  if(status_out) {
    *status_out = SHM_REGION_CREATE_OK;
  }
  if(size == 0) {
    if(status_out) {
      *status_out = SHM_REGION_CREATE_INVALID_ARGUMENT;
    }
    return NULL;
  }

  size_t page_count = align_page_count(size);
  if(page_count == 0) {
    if(status_out) {
      *status_out = SHM_REGION_CREATE_INVALID_ARGUMENT;
    }
    return NULL;
  }

  shm_region_t* region = kmalloc(sizeof(shm_region_t));
  if(region == NULL) {
    if(status_out) {
      *status_out = SHM_REGION_CREATE_NO_MEMORY;
    }
    return NULL;
  }

  memset(region, 0, sizeof(shm_region_t));
  kmutex_init(&region->lock);
  region->key = key;
  region->size_bytes = size;
  region->page_count = page_count;
  region->mode = mode;
  region->marked_for_removal = false;
  region->refcount = 1; // manager reference

  if(!shm_region_allocate_pages(region)) {
    if(status_out) {
      *status_out = SHM_REGION_CREATE_NO_MEMORY;
    }
    kfree(region);
    return NULL;
  }

  if(shm_manager_insert(region) == NULL) {
    if(status_out) {
      *status_out = SHM_REGION_CREATE_NO_MEMORY;
    }
    for(size_t i = 0; i < region->page_count; ++i) {
      if(free_pages_fn) {
        free_pages_fn(phys_frame_from_addr(region->pages[i]), 1);
      }
    }
    kfree(region->pages);
    kfree(region);
    return NULL;
  }

  if(id_out) {
    *id_out = region->id;
  }

  return region;
}

static shm_region_t* shm_manager_find_locked(bool by_key,
                                             shm_key_t key,
                                             int id) {
  shm_region_t* result = NULL;
  kmutex_lock(&shm_manager.lock);
  for(shm_region_t* it = shm_manager.head; it != NULL; it = it->next) {
    if(by_key) {
      if(it->key == key && !it->marked_for_removal) {
        result = it;
        break;
      }
    } else {
      if(it->id == id) {
        result = it;
        break;
      }
    }
  }
  if(result != NULL) {
    shm_region_ref(result);
  }
  kmutex_unlock(&shm_manager.lock);
  return result;
}

shm_region_t* shm_region_get_by_id(int id) {
  if(id <= 0) {
    return NULL;
  }
  return shm_manager_find_locked(false, 0, id);
}

shm_region_t* shm_region_get_by_key(shm_key_t key) {
  return shm_manager_find_locked(true, key, 0);
}

void shm_region_ref(shm_region_t* region) {
  if(region == NULL) {
    return;
  }
  __atomic_add_fetch(&region->refcount, 1, __ATOMIC_SEQ_CST);
}

void shm_region_unref(shm_region_t* region) {
  if(region == NULL) {
    return;
  }
  size_t refs = __atomic_sub_fetch(&region->refcount, 1, __ATOMIC_SEQ_CST);
  if(refs == 0) {
    shm_manager_remove(region);
    shm_region_destroy(region);
  }
}

int shm_region_id(const shm_region_t* region) {
  return region ? region->id : -1;
}

shm_key_t shm_region_key(const shm_region_t* region) {
  return region ? region->key : -1;
}

size_t shm_region_size(const shm_region_t* region) {
  return region ? region->size_bytes : 0;
}

size_t shm_region_page_count(const shm_region_t* region) {
  return region ? region->page_count : 0;
}

phys_addr_t shm_region_page(const shm_region_t* region, size_t index) {
  if(region == NULL || index >= region->page_count) {
    return 0;
  }
  return region->pages[index];
}

uint16_t shm_region_mode(const shm_region_t* region) {
  return region ? region->mode : 0;
}

void shm_region_set_marked_for_removal(shm_region_t* region, bool value) {
  if(region == NULL) {
    return;
  }
  bool release = false;
  kmutex_lock(&region->lock);
  region->marked_for_removal = value;
  if(value && region->attachment_count == 0) {
    release = true;
  }
  kmutex_unlock(&region->lock);
  if(release) {
    shm_region_unref(region);
  }
}

bool shm_region_marked_for_removal(shm_region_t* region) {
  if(region == NULL) {
    return false;
  }
  bool marked;
  kmutex_lock(&region->lock);
  marked = region->marked_for_removal;
  kmutex_unlock(&region->lock);
  return marked;
}

void shm_region_increment_attachments(shm_region_t* region) {
  if(region == NULL) {
    return;
  }
  kmutex_lock(&region->lock);
  region->attachment_count++;
  kmutex_unlock(&region->lock);
}

void shm_region_decrement_attachments(shm_region_t* region) {
  if(region == NULL) {
    return;
  }
  bool release = false;
  kmutex_lock(&region->lock);
  if(region->attachment_count > 0) {
    region->attachment_count--;
  }
  if(region->attachment_count == 0 && region->marked_for_removal) {
    release = true;
  }
  kmutex_unlock(&region->lock);
  if(release) {
    shm_region_unref(region);
  }
}

size_t shm_region_attachment_count(shm_region_t* region) {
  if(region == NULL) {
    return 0;
  }
  size_t count;
  kmutex_lock(&region->lock);
  count = region->attachment_count;
  kmutex_unlock(&region->lock);
  return count;
}
