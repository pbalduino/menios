#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/heap.h>
#include <kernel/hw.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>
#include <uacpi/types.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>

#include <stdio.h>
#include <string.h>

static hardware_device_p devices_head;

static void hardware_device_register(const char* path, const char* hid, driver_p driver) {
  hardware_device_p node = (hardware_device_p)kmalloc(sizeof(hardware_device_t));
  if(node == NULL) {
    errk("hardware_device_register: allocation failed for %s\n", path);
    return;
  }
  memset(node, 0, sizeof(*node));
  strncpy(node->path, path ? path : "?", sizeof(node->path) - 1);
  strncpy(node->hid, hid ? hid : "?", sizeof(node->hid) - 1);
  node->driver = driver;
  node->next = devices_head;
  devices_head = node;
}

hardware_device_p hardware_devices(void) {
  return devices_head;
}

void hardware_log_devices(void) {
  hardware_device_p node = devices_head;
  while(node) {
    const char* driver_name = node->driver ? node->driver->name : "(no driver)";
    logk("  Device %-40s HID=%-8s Driver=%s\n", node->path, node->hid, driver_name);
    node = node->next;
  }
}

static uacpi_ns_iteration_decision register_device(void *ctx, uacpi_namespace_node* node) {
  uacpi_namespace_node_info* info;
  uacpi_status ret = uacpi_get_namespace_node_info(node, &info);
  
  if(uacpi_unlikely_error(ret)) {
    const char *path = uacpi_namespace_node_generate_absolute_path(node);
    serial_printf("register_device: unable to retrieve node %s information: %s\n", path, uacpi_status_to_string(ret));
    uacpi_free_absolute_path(path);
    uacpi_free_namespace_node_info(info);
    return UACPI_NS_ITERATION_DECISION_CONTINUE;
  }

  if(info->type != UACPI_OBJECT_DEVICE) {
    uacpi_free_namespace_node_info(info);
    return UACPI_NS_ITERATION_DECISION_CONTINUE;
  }

  const char* path = uacpi_namespace_node_generate_absolute_path(node);

  if(info->flags & UACPI_NS_NODE_INFO_HAS_HID) {
    if(info->flags & UACPI_NS_NODE_INFO_HAS_UID) {
      serial_printf("register_device: '%s' has HID and UID - '%s' - '%s'\n", path, info->hid.value, info->uid.value);
      // errk("  Found device '%s' with HID '%s[%s]' but no driver was found\n", path, info->hid.value, info->uid.value);
    } else {
      serial_printf("register_device: '%s' has HID - '%s'\n", path, info->hid.value);
      // errk("  Found device '%s' with HID '%s' but no driver was found\n", path, info->hid.value);
    }

    driver_p driver = driver_load(info->hid.value);
    hardware_device_register(path, info->hid.value, driver);
  }

  uacpi_free_absolute_path(path);

  return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

void acpi_enumerate() {
  logk("  Enumerating devices from ACPI table\n");
  uacpi_namespace_for_each_node_depth_first(uacpi_namespace_root(), register_device, UACPI_NULL);
}

void hardware_init() {
  devices_head = NULL;
  driver_init();
  logk("Probing hardware\n");
  acpi_enumerate();
  hardware_log_devices();
}
