#include <kernel/devicetree.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

static volatile struct limine_dtb_request dtb_request = {
  .id = LIMINE_DTB_REQUEST,
  .revision = 3
};


void read_device_tree() {
  if(dtb_request.response == NULL || dtb_request.response->dtb_ptr == NULL) {
    serial_error("read_device_tree: No device tree available\n");
    return;
  }

  fdt_header* fdt = (fdt_header*)dtb_request.response->dtb_ptr;

  serial_printf("read_device_tree: %lx\n", fdt->magic);
}