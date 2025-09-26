#include <errno.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/syscall.h>
#include <string.h>

#define SYSCALL_MAX 256

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame);
static uint64_t syscall_write_handler(syscall_frame_t* frame);
static uint64_t syscall_exit_handler(syscall_frame_t* frame);

static syscall_handler_t syscall_table[SYSCALL_MAX];

static void syscall_register(uint64_t number, syscall_handler_t handler) {
  if(number >= SYSCALL_MAX) {
    serial_printf("syscall_register: number %lu out of range\n", number);
    return;
  }
  syscall_table[number] = handler ? handler : syscall_stub_unimplemented;
}

void syscall_init(void) {
  for(size_t i = 0; i < SYSCALL_MAX; i++) {
    syscall_table[i] = syscall_stub_unimplemented;
  }

  syscall_register(SYS_WRITE, syscall_write_handler);
  syscall_register(SYS_EXIT, syscall_exit_handler);

  serial_printf("syscall_init: initialized dispatcher (INT 0x80)\n");
}

uint64_t syscall_dispatch(syscall_frame_t* frame) {
  uint64_t number = frame->rax;

  if(number < SYSCALL_MAX) {
    syscall_handler_t handler = syscall_table[number];
    if(handler) {
      uint64_t result = handler(frame);
      frame->rax = result;
      return result;
    }
  }

  frame->rax = (uint64_t)(-ENOSYS);
  return frame->rax;
}

static uint64_t syscall_stub_unimplemented(syscall_frame_t* frame) {
  serial_printf("syscall_stub_unimplemented: number %lu not implemented\n", frame->rax);
  return (uint64_t)(-ENOSYS);
}

static uint64_t syscall_write_handler(syscall_frame_t* frame) {
  int fd = (int)frame->rdi;
  const char* buffer = (const char*)frame->rsi;
  size_t length = (size_t)frame->rdx;

  if(fd != 1) {
    return (uint64_t)(-EBADF);
  }

  if(buffer == NULL) {
    return (uint64_t)(-EFAULT);
  }

  for(size_t i = 0; i < length; i++) {
    char ch = buffer[i];
    kputchar((int)ch);
  }

  return (uint64_t)length;
}

static uint64_t syscall_exit_handler(syscall_frame_t* frame) {
  int status = (int)frame->rdi;
  proc_exit(status);
  proc_switch(frame);
  return 0;
}
