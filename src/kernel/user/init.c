#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/block_device.h>
#include <kernel/console.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/fs/vfs/vfs.h>

extern const uint8_t init_elf_start[];
extern const uint8_t init_elf_end[];

static block_device_t* init_find_root_device(void) {
  block_device_t* device = block_device_first();
  while(device != NULL) {
    if(device->block_size != 0 && strncmp(device->name, "sata", 4) == 0) {
      return device;
    }
    device = block_device_next(device);
  }
  return NULL;
}

static void init_mount_root(void) {
  block_device_t* device = init_find_root_device();
  if(device == NULL) {
    serial_printf("init: no SATA device available for root mount\n");
    return;
  }

  if(vfs_mount_fat32_root(device)) {
    serial_printf("init: mounted FAT32 root on '%s'\n", device->name);
  } else {
    serial_printf("init: failed to mount FAT32 root on '%s'\n", device->name);
  }
}

void user_init_launch(void) {
  size_t code_size = (size_t)(init_elf_end - init_elf_start);
  if(code_size == 0) {
    serial_printf("user_init_launch: init ELF is empty\n");
    return;
  }

  proc_info_p proc = kmalloc(sizeof(proc_info_t));
  if(proc == NULL) {
    serial_printf("user_init_launch: failed to allocate proc_info\n");
    return;
  }
  memset(proc, 0, sizeof(proc_info_t));

  init_mount_root();

  proc_create_user(proc, "init", init_elf_start, code_size, NULL);
  proc_set_priority(proc, PROC_PRIO_NORMAL);
  serial_printf("user_init_launch: queued init process pid %u\n", proc->pid);
  proc_execute(proc);

  logk("[init] queued PID %u\n", proc->pid);
}
