#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/user_mode.h>

#define USER_DEMO_STACK_SIZE (16 * 1024)

static void user_demo_entry(void* arg) __attribute__((noreturn));

static void user_demo_entry(void* arg) {
  (void)arg;

  static const char message[] = "Hello from Ring 3 via int 0x80!\n";
  const uint64_t length = sizeof(message) - 1;

  __asm__ __volatile__(
    "mov $1, %%rax\n\t"
    "mov $1, %%rdi\n\t"
    "mov %0, %%rsi\n\t"
    "mov %1, %%rdx\n\t"
    "int $0x80\n\t"
    "mov $60, %%rax\n\t"
    "xor %%rdi, %%rdi\n\t"
    "int $0x80\n\t"
    :
    : "r"(message), "r"(length)
    : "rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory");

  __builtin_unreachable();
}

void user_demo_launch(void) {
  proc_info_p proc = kmalloc(sizeof(proc_info_t));
  if(proc == NULL) {
    serial_printf("user_demo_launch: failed to allocate proc_info\n");
    return;
  }
  memset(proc, 0, sizeof(proc_info_t));

  serial_printf("user_demo_launch: scheduling user demo process\n");
  proc_create_user(proc, "user_demo", user_demo_entry, NULL);
  proc_execute(proc);
}
