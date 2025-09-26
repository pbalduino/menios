#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t user_demo_stub_start[];
extern const uint8_t user_demo_stub_end[];

void user_demo_launch(void) {
  proc_info_p proc = kmalloc(sizeof(proc_info_t));
  if(proc == NULL) {
    serial_printf("user_demo_launch: failed to allocate proc_info\n");
    return;
  }
  memset(proc, 0, sizeof(proc_info_t));

  size_t code_size = (size_t)(user_demo_stub_end - user_demo_stub_start);
  if(code_size == 0) {
    serial_printf("user_demo_launch: stub size is zero\n");
    kfree(proc);
    return;
  }

  serial_printf("user_demo_launch: scheduling user demo process\n");
  proc_create_user(proc, "user_demo", user_demo_stub_start, code_size, NULL);
  proc_execute(proc);
}
