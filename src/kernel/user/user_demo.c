#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/block_device.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t user_demo_elf_start[];
extern const uint8_t user_demo_elf_end[];

typedef struct user_demo_spec_t {
  const char* name;
  uint8_t     priority;
} user_demo_spec_t;

static const user_demo_spec_t demo_specs[] = {
  { "user_demo_low",     PROC_PRIO_LOW },
  { "user_demo_normal",  PROC_PRIO_NORMAL },
  { "user_demo_high",    PROC_PRIO_HIGH }
};

#define USER_DEMO_COUNT (sizeof(demo_specs) / sizeof(demo_specs[0]))

static void user_demo_block_probe(void) {
  block_device_t* device = block_device_first();
  while(device != NULL) {
    if(device->block_size != 0 && strncmp(device->name, "sata", 4) == 0) {
      break;
    }
    device = block_device_next(device);
  }

  if(device == NULL) {
    serial_printf("user_demo_launch: no SATA device available for probe\n");
    return;
  }

  size_t block_size = device->block_size ? device->block_size : 512u;
  uint8_t* buffer = kmalloc(block_size);
  if(buffer == NULL) {
    serial_printf("user_demo_launch: failed to allocate SATA probe buffer\n");
    return;
  }

  bool ok = block_device_read(device, 0, buffer, 1);
  if(ok) {
    serial_printf("user_demo_launch: '%s' LBA0 first 16 bytes: ", device->name);
    size_t preview = block_size < 16 ? block_size : 16;
    for(size_t i = 0; i < preview; i++) {
      serial_printf("%02x%s", buffer[i], (i + 1 < preview) ? " " : "");
    }
    serial_printf("\n");
  } else {
    serial_printf("user_demo_launch: SATA read failed on '%s'\n", device->name);
  }

  kfree(buffer);
}

void user_demo_launch(void) {
  size_t code_size = (size_t)(user_demo_elf_end - user_demo_elf_start);
  if(code_size == 0) {
    serial_printf("user_demo_launch: stub size is zero\n");
    return;
  }

  user_demo_block_probe();

  serial_printf("user_demo_launch: scheduling %lu user demo processes\n",
                (unsigned long)USER_DEMO_COUNT);

  for(size_t i = 0; i < USER_DEMO_COUNT; i++) {
    proc_info_p proc = kmalloc(sizeof(proc_info_t));
    if(proc == NULL) {
      serial_printf("user_demo_launch: failed to allocate proc_info (%zu)\n", i);
      return;
    }
    memset(proc, 0, sizeof(proc_info_t));

    proc_create_user(proc,
                     demo_specs[i].name,
                     user_demo_elf_start,
                     code_size,
                     (void*)(uintptr_t)i);

    proc_set_priority(proc, demo_specs[i].priority);
    serial_printf("user_demo_launch: queued %s with priority %u\n",
                  demo_specs[i].name,
                  demo_specs[i].priority);

    proc_execute(proc);
  }
}
