#include <kernel/arch/x86_64/gdt.h>
#include <kernel/msr.h>
#include <kernel/serial.h>
#include <kernel/syscall_entry.h>

#ifndef MENIOS_HOST_TEST
extern void syscall_entry(void);
#endif

#define SYSCALL_CONTEXT_FIELD(name, offset) \
  enum { SYSCALL_CONTEXT_##name = (offset) }

SYSCALL_CONTEXT_FIELD(KERNEL_RSP, 0);
SYSCALL_CONTEXT_FIELD(USER_RSP, 8);
SYSCALL_CONTEXT_FIELD(USER_RIP, 16);
SYSCALL_CONTEXT_FIELD(USER_RFLAGS, 24);

typedef struct syscall_cpu_context_t {
  uint64_t kernel_rsp;
  uint64_t user_rsp;
  uint64_t user_rip;
  uint64_t user_rflags;
} syscall_cpu_context_t;

static syscall_cpu_context_t syscall_cpu_context __attribute__((aligned(16)));

uint64_t syscall_last_return_value = 0;
uint64_t syscall_last_return_slot_value = 0;

void syscall_arch_initialize(void) {
#ifdef MENIOS_HOST_TEST
  (void)0;
#else
  const uint64_t star_kernel_cs = (uint64_t)KERNEL_CODE_SEGMENT;
  const uint64_t star_user_cs = (uint64_t)USER_CODE_SEGMENT;
  const uint64_t star_value = (star_kernel_cs << 32) | (star_user_cs << 48);

  uint64_t efer = msr_read(IA32_EFER);
  efer |= IA32_EFER_SCE;
  msr_write(IA32_EFER, efer);

  msr_write(IA32_STAR, star_value);
  msr_write(IA32_LSTAR, (uint64_t)&syscall_entry);

  const uint64_t rflags_mask = (1ull << 9) | (1ull << 8) | (1ull << 10); // IF, TF, DF
  msr_write(IA32_FMASK, rflags_mask);

  msr_write(IA32_GS_BASE, 0);
  msr_write(IA32_KERNEL_GS_BASE, (uint64_t)&syscall_cpu_context);

  syscall_cpu_context.kernel_rsp = gdt_get_tss_stack_top();
  syscall_cpu_context.user_rsp = 0;
  syscall_cpu_context.user_rip = 0;
  syscall_cpu_context.user_rflags = 0;

  serial_printf("syscall_arch_initialize: fast syscall/sysret configured (LSTAR=%p)\n", &syscall_entry);
#endif
}

void syscall_set_kernel_stack(uint64_t rsp) {
#ifdef MENIOS_HOST_TEST
  (void)rsp;
#else
  syscall_cpu_context.kernel_rsp = rsp;
#endif
}
