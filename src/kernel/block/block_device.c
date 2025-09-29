#include <kernel/block_device.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

#include <string.h>

static kmutex_t block_device_lock;
static block_device_t* block_device_head = NULL;
static bool block_device_initialized = false;

void block_device_system_init(void) {
  kmutex_init(&block_device_lock);
  block_device_head = NULL;
  block_device_initialized = true;
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

static bool block_device_validate(block_device_t* device) {
  return device && device->ops && device->ops->read_blocks && device->block_size != 0;
}

bool block_device_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  if(!block_device_validate(device) || buffer == NULL || block_count == 0) {
    return false;
  }
  if(lba + block_count > device->block_count) {
    return false;
  }
  return device->ops->read_blocks(device, lba, buffer, block_count);
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
  return device->ops->write_blocks(device, lba, buffer, block_count);
}

bool block_device_flush(block_device_t* device) {
  if(device == NULL || device->ops == NULL || device->ops->flush == NULL) {
    return true;
  }
  return device->ops->flush(device);
}
