#include <kernel/ioport.h>

#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ioport_region {
  uint16_t base;
  uint16_t length;
  struct ioport_region* next;
};

static spinlock_t ioport_lock;
static ioport_region_t* ioport_regions = NULL;
static bool ioport_initialized = false;

static bool ioport_ranges_overlap(uint16_t base_a,
                                 uint16_t length_a,
                                 uint16_t base_b,
                                 uint16_t length_b) {
  uint32_t start_a = base_a;
  uint32_t end_a = start_a + length_a - 1;
  uint32_t start_b = base_b;
  uint32_t end_b = start_b + length_b - 1;
  return !(end_a < start_b || end_b < start_a);
}

void ioport_manager_initialize(void) {
  if(ioport_initialized) {
    return;
  }

  spinlock_init(&ioport_lock);
  ioport_regions = NULL;
  ioport_initialized = true;
}

int ioport_reserve(uint16_t base,
                   uint16_t length,
                   ioport_region_t** out_region) {
  if(!ioport_initialized || out_region == NULL || length == 0) {
    return -EINVAL;
  }

  uint32_t range_end = (uint32_t)base + (uint32_t)length;
  if(range_end > 0x10000u) {
    return -ERANGE;
  }

  spinlock_lock(&ioport_lock);

  for(ioport_region_t* current = ioport_regions; current != NULL; current = current->next) {
    if(ioport_ranges_overlap(base, length, current->base, current->length)) {
      spinlock_unlock(&ioport_lock);
      return -EBUSY;
    }
  }

  ioport_region_t* region = kmalloc(sizeof(*region));
  if(region == NULL) {
    spinlock_unlock(&ioport_lock);
    return -ENOMEM;
  }

  region->base = base;
  region->length = length;
  region->next = ioport_regions;
  ioport_regions = region;

  spinlock_unlock(&ioport_lock);

  *out_region = region;
  return 0;
}

void ioport_release(ioport_region_t* region) {
  if(!ioport_initialized || region == NULL) {
    return;
  }

  spinlock_lock(&ioport_lock);

  ioport_region_t** link = &ioport_regions;
  while(*link != NULL) {
    if(*link == region) {
      *link = region->next;
      break;
    }
    link = &(*link)->next;
  }

  spinlock_unlock(&ioport_lock);

  kfree(region);
}

static bool ioport_region_contains(ioport_region_t* region,
                                   uint16_t offset,
                                   uint8_t width) {
  if(region == NULL) {
    return false;
  }

  uint32_t start = region->base;
  uint32_t end = start + region->length;
  uint32_t request = (uint32_t)region->base + offset;
  uint32_t request_end = request + width;

  return (request >= start) && (request_end <= end);
}

int ioport_read(ioport_region_t* region,
                uint16_t offset,
                uint8_t width,
                uint64_t* out_value) {
  if(region == NULL || out_value == NULL) {
    return -EINVAL;
  }

  if(width != 1 && width != 2 && width != 4) {
    return -EINVAL;
  }

  if(!ioport_region_contains(region, offset, width)) {
    return -ERANGE;
  }

  uint16_t port = region->base + offset;

  switch(width) {
    case 1:
      *out_value = inb(port);
      break;
    case 2:
      *out_value = inw(port);
      break;
    case 4:
      *out_value = inl(port);
      break;
    default:
      return -EINVAL;
  }

  return 0;
}

int ioport_write(ioport_region_t* region,
                 uint16_t offset,
                 uint8_t width,
                 uint64_t value) {
  if(region == NULL) {
    return -EINVAL;
  }

  if(width != 1 && width != 2 && width != 4) {
    return -EINVAL;
  }

  if(!ioport_region_contains(region, offset, width)) {
    return -ERANGE;
  }

  uint16_t port = region->base + offset;

  switch(width) {
    case 1:
      outb(port, (uint8_t)value);
      break;
    case 2:
      outw(port, (uint16_t)value);
      break;
    case 4:
      outl(port, (uint32_t)value);
      break;
    default:
      return -EINVAL;
  }

  return 0;
}
