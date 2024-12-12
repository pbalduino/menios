#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/devicetree.h>
#include <kernel/heap.h>
#include <kernel/hw.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>
#include <uacpi/types.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>

#include <stdio.h>

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

    driver_load(info->hid.value);
  }

  uacpi_free_absolute_path(path);

  return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

void acpi_enumerate() {
  logk("  Enumerating devices from ACPI table\n");
  uacpi_namespace_for_each_node_depth_first(uacpi_namespace_root(), register_device, UACPI_NULL);
}

void hardware_init() {
  driver_init();
  logk("Probing hardware\n");
  serial_printf("hardware_init: Reading device tree\n");
  read_device_tree();
  // serial_printf("hardware_init: Probing hardware via ACPI\n");
  acpi_enumerate();
}