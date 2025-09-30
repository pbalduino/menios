#include <stdint.h>
#include <string.h>

#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t mosh_elf_start[];
extern const uint8_t mosh_elf_end[];

void user_mosh_launch(void) {
  size_t code_size = (size_t)(mosh_elf_end - mosh_elf_start);
  if(code_size == 0) {
    serial_printf("user_mosh_launch: mosh ELF is empty\n");
    return;
  }

  proc_info_p proc = kmalloc(sizeof(proc_info_t));
  if(proc == NULL) {
    serial_printf("user_mosh_launch: failed to allocate proc_info\n");
    return;
  }
  memset(proc, 0, sizeof(proc_info_t));

  proc_create_user(proc, "mosh", mosh_elf_start, code_size, NULL);
  proc_env_set(proc, "PATH", "/bin:/", true);
  proc_env_set(proc, "HOME", "/", true);
  proc_env_set(proc, "PS1", "mosh$ ", true);
  proc_set_priority(proc, PROC_PRIO_NORMAL);
  serial_printf("user_mosh_launch: queued mosh process pid %u\n", proc->pid);
  proc_execute(proc);
}
