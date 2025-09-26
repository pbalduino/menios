#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/heap.h>
#include <kernel/serial.h>
#include <kernel/user_mode.h>

#define USER_DEMO_STACK_SIZE (16 * 1024)

static void user_demo_entry(void* arg) {
  (void)arg;
  volatile unsigned long counter = 0;
  while(true) {
    counter++;
  }
}

void user_demo_launch(void) {
  void* stack = kmalloc(USER_DEMO_STACK_SIZE);
  if(stack == NULL) {
    serial_printf("user_demo_launch: failed to allocate stack\n");
    return;
  }

  memset(stack, 0, USER_DEMO_STACK_SIZE);
  void* stack_top = (uint8_t*)stack + USER_DEMO_STACK_SIZE;

  serial_printf("user_demo_launch: entering Ring 3 demo\n");
  user_mode_enter(user_demo_entry, stack_top, 0);
  serial_printf("user_demo_launch: returned unexpectedly from user mode\n");
}
