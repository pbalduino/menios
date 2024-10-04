#include <boot/limine.h>
#include <errno.h>
#include <stdio.h>
#include <types.h>
#include <kernel/acpi.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>

#define UACPI_ARENA_SIZE (PAGE_SIZE)

static uint8_t uacpi_arena[UACPI_ARENA_SIZE];

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
/*
  * Shut down right here using the helper we have defined above.
  *
  * Note that it's generally terrible practice to run any AML from
  * an interrupt handler, as it's allowed to allocate, map, sleep,
  * stall, acquire mutexes, etc. So, if possible in your kernel,
  * instead schedule the shutdown callback to be run in a normal
  * preemptible context later.
  */
  serial_printf("power button pressed\n");
  printf("Power button pressed.\n");

  acpi_shutdown();
  return UACPI_INTERRUPT_HANDLED;
}

int power_button_init(void) {
  serial_printf("power_button_init: Initializing power button.\n");

  uacpi_status ret = uacpi_install_fixed_event_handler(
      UACPI_FIXED_EVENT_POWER_BUTTON,
      handle_power_button, UACPI_NULL);

  if (uacpi_unlikely_error(ret)) {
      serial_printf("failed to install power button event callback: %s", uacpi_status_to_string(ret));
      return -ENODEV;
  }

  serial_printf("power_button_init: Done.\n");
  return 0;
}

int acpi_init() {
  printf("- Initializing ACPI.");
  serial_printf("acpi_init: Initializing ACPI.\n");

  uacpi_setup_early_table_access((void*)uacpi_arena, UACPI_ARENA_SIZE);
  printf(".");

  uacpi_status ret = uacpi_initialize(0);
  if (uacpi_unlikely_error(ret)) {
    serial_printf("uacpi_initialize error: %s", uacpi_status_to_string(ret));
    return -ENODEV;
  }
  printf(".");

  ret = uacpi_namespace_load();
  if (uacpi_unlikely_error(ret)) {
    serial_printf("uacpi_namespace_load error: %s\n", uacpi_status_to_string(ret));
    return -ENODEV;
  }
  printf(".");

  ret = uacpi_namespace_initialize();
  if(uacpi_unlikely_error(ret)) {
    serial_printf("uacpi_namespace_initialize error: %s\n", uacpi_status_to_string(ret));
    return -ENODEV;
  }
  printf(".");

  ret = uacpi_finalize_gpe_initialization();
  if (uacpi_unlikely_error(ret)) {
    serial_printf("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
    return -ENODEV;
  }

  ret = power_button_init();
  if (uacpi_unlikely_error(ret)) {
    serial_printf("power_button_init error: %s", uacpi_status_to_string(ret));
    return -ENODEV;
  }

  printf(".OK\n");
  serial_printf("acpi_init: uacpi initialized.\n");

  return 0;
}

int acpi_shutdown() {
  printf("- Shutting down.\n");
  serial_printf("acpi_shutdown: Shutting down.\n");

  uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
  if (uacpi_unlikely_error(ret)) {
    printf("failed to prepare for sleep: %s", uacpi_status_to_string(ret));
    return -EIO;
  }

  cli();

  ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
  if (uacpi_unlikely_error(ret)) {
    printf("failed to enter sleep: %s", uacpi_status_to_string(ret));
    return -EIO;
  }

  serial_printf("acpi_shutdown: this line should't happen.\n");

  return 0;

}