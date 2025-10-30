#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/driver/ps2kb.h>
#include <kernel/driver/pciroot.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <string.h>

static driver_list_p driver_list;

void driver_init() {
  logk("Initing driver lookup table\n");
  serial_line("Initing driver lookup table");
  driver_list = NULL;

  ps2kb_init();
  pciroot_init();
}

void driver_register(driver_p driver) {
  serial_line("Registering new driver");

  driver_p new_driver = kmalloc(sizeof(driver_t));
  driver_list_p new_node = kmalloc(sizeof(driver_list_t));

  memcpy(new_driver, driver, sizeof(driver_t));
  new_node->driver = new_driver;
  new_node->next = driver_list;
  driver_list = new_node;
  serial_printf("driver_register: Registered driver '%s' for HID '%s'\n", driver->name, driver->hid);
}

driver_p driver_load(const char* hid) {
  driver_list_p node = driver_list;

  while(node) {
    if(strncmp(node->driver->hid, hid, 12) == 0) {
      logk("  Found driver '%s' for HID '%s'\n", node->driver->name, hid);
      node->driver->start();
      return node->driver;
    }
    node = node->next;
  }
  return NULL;
}
