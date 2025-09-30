#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/char_device.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

static kmutex_t char_device_lock;
static char_device_t* char_device_head = NULL;
static bool char_device_initialized = false;

static bool char_device_name_exists(const char* name) {
  for(char_device_t* node = char_device_head; node != NULL; node = node->next) {
    if(strncmp(node->name, name, CHAR_DEVICE_NAME_MAX) == 0) {
      return true;
    }
  }
  return false;
}

void char_device_system_init(void) {
  kmutex_init(&char_device_lock);
  char_device_head = NULL;
  char_device_initialized = true;
}

bool char_device_register(char_device_t* device) {
  if(!char_device_initialized || device == NULL || device->ops == NULL || device->ops->open == NULL) {
    return false;
  }

  if(device->name[0] == '\0') {
    return false;
  }

  kmutex_lock(&char_device_lock);
  if(char_device_name_exists(device->name)) {
    kmutex_unlock(&char_device_lock);
    serial_printf("char_device_register: '%s' already registered\n", device->name);
    return false;
  }

  device->next = char_device_head;
  char_device_head = device;
  kmutex_unlock(&char_device_lock);

  serial_printf("char_device_register: registered '%s'\n", device->name);
  return true;
}

void char_device_unregister(char_device_t* device) {
  if(!char_device_initialized || device == NULL) {
    return;
  }

  kmutex_lock(&char_device_lock);
  char_device_t** prev = &char_device_head;
  while(*prev != NULL) {
    if(*prev == device) {
      *prev = device->next;
      device->next = NULL;
      break;
    }
    prev = &(*prev)->next;
  }
  kmutex_unlock(&char_device_lock);
}

char_device_t* char_device_lookup(const char* name) {
  if(!char_device_initialized || name == NULL) {
    return NULL;
  }

  kmutex_lock(&char_device_lock);
  char_device_t* node = char_device_head;
  while(node) {
    if(strncmp(node->name, name, CHAR_DEVICE_NAME_MAX) == 0) {
      kmutex_unlock(&char_device_lock);
      return node;
    }
    node = node->next;
  }
  kmutex_unlock(&char_device_lock);
  return NULL;
}

int char_device_open(const char* name, uint32_t mode, file_t** out_file) {
  if(!char_device_initialized || name == NULL || out_file == NULL) {
    return -EINVAL;
  }

  char_device_t* device = NULL;

  kmutex_lock(&char_device_lock);
  for(char_device_t* node = char_device_head; node != NULL; node = node->next) {
    if(strncmp(node->name, name, CHAR_DEVICE_NAME_MAX) == 0) {
      device = node;
      break;
    }
  }
  kmutex_unlock(&char_device_lock);

  if(device == NULL) {
    return -ENODEV;
  }

  uint32_t requested_mode = mode;
  if(requested_mode == 0) {
    requested_mode = FILE_MODE_READ;
  }

  if(device->supported_modes != 0 && (requested_mode & device->supported_modes) != requested_mode) {
    return -EACCES;
  }

  int rc = device->ops->open(device, requested_mode, out_file);
  if(rc < 0) {
    return rc;
  }

  if(rc == 0 && out_file != NULL && *out_file != NULL && (*out_file)->mode == 0) {
    uint32_t default_mode = device->supported_modes;
    if(default_mode == 0) {
      default_mode = FILE_MODE_READ | FILE_MODE_WRITE;
    }
    (*out_file)->mode = default_mode;
  }

  return rc;
}

void char_device_for_each(char_device_iter_t iter, void* context) {
  if(!char_device_initialized || iter == NULL) {
    return;
  }

  kmutex_lock(&char_device_lock);
  for(char_device_t* node = char_device_head; node != NULL; node = node->next) {
    if(!iter(node, context)) {
      break;
    }
  }
  kmutex_unlock(&char_device_lock);
}
