#ifdef USERMODE
#define STDOUT_FILENO 1
#define SYS_write 1

#include <stddef.h>

void kernel_write(const char* message, size_t length) {
  // Use the write syscall
  asm volatile (
    "mov $1, %%rax \n"      // syscall number (SYS_write)
    "mov $1, %%rdi \n"      // file descriptor (STDOUT_FILENO)
    "mov %0, %%rsi \n"      // pointer to the message
    "mov %1, %%rdx \n"      // length of the message
    "syscall"
    :
    : "r"(message), "r"(length)
    : "rax", "rdi", "rsi", "rdx"
  );
}

void _start() {
  const char* message = "- Usermode: OK.\n";
  size_t length = 16;
  kernel_write(message, length);
}
#endif
