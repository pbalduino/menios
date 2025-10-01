#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/console.h>
#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t init_elf_start[];
extern const uint8_t init_elf_end[];

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

  proc_create_user(proc, "init", init_elf_start, code_size, NULL);
  proc_set_priority(proc, PROC_PRIO_NORMAL);
  serial_printf("user_init_launch: queued init process pid %u\n", proc->pid);
  proc_execute(proc);

  logk("[init] queued PID %u\n", proc->pid);
}
