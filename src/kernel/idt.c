#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

static idt_pointer_t idt_p; // __attribute__((aligned(8)));
static idt_entry_t idt[0x100]; // __attribute__((aligned(4096)));

static void gpf_log(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  va_start(args, fmt);
  serial_vprintf(fmt, args);
  va_end(args);
}

static const char *gpf_table_name(uint8_t table_bits) {
  switch (table_bits & 0x3) {
    case 0: return "GDT";
    case 1: return "IDT";
    case 2: return "LDT";
    default: return "IDT";
  }
}

void idt_add_isr(int interruption, void* handler) {
  // Set up the IDT entry
  uintptr_t handler_address = (uintptr_t)handler;

  idt[interruption].base_low = (uint16_t)(handler_address & 0xFFFF);
  idt[interruption].selector = KERNEL_CODE_SEGMENT; // Your code segment selector
  idt[interruption].ist = 0;   // Set to 0 for most cases
  idt[interruption].type_attr = 0x8e; // 0x8e indicates an interrupt gate (64-bit interrupt gate)
  idt[interruption].base_mid = (uint16_t)((handler_address >> 16) & 0xffff);
  idt[interruption].base_high = (uint32_t)((handler_address >> 32) & 0xffffffff);
  idt[interruption].reserved = 0;
}

void idt_init() {
  logk("Setting IDT");
  idt_p.size = sizeof(idt) - 1;
  idt_p.offset = (uintptr_t)&idt;
  serial_printf("idt_init: idt_p @ %p - offset: %lx\n", idt_p, idt_p.offset);
  idt_add_isr(ISR_DIVISION_BY_ZERO, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DEBUG, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_BREAKPOINT, &idt_generic_isr_asm_handler);
  idt_add_isr(ISR_DOUBLE_FAULT, &idt_df_isr_asm_handler);
  idt_add_isr(ISR_GENERAL_PROTECTION_FAULT, &idt_gpf_isr_asm_handler);
  idt_add_isr(ISR_PAGE_FAULT, &idt_pf_isr_asm_handler);
  idt_add_isr(ISR_PERIODIC_TIMER, &idt_period_timer_isr_asm_handler);
  idt_add_isr(ISR_KEYBOARD, &ps2kb_isr_handler);

  idt_load(&idt_p);

  puts(".OK\n");
}

void idt_generic_isr_handler() {
  puts("= Exception caught.");
  serial_printf("== Exception caught ==\n");
  halt();
}

void idt_df_isr_handler() {
  puts("= Double fault caught.\n");
  serial_printf("== Double fault raised ==\n");
  halt();
}

void idt_gpf_isr_handler(idt_exception_p cpu_state) {
  uint64_t *frame = &cpu_state->error_code;
  const uint64_t error_code = frame[0];
  const uint64_t fault_rip = frame[1];
  const uint64_t fault_cs  = frame[2];
  const uint64_t fault_rflags = frame[3];
  const bool privilege_transition = (fault_cs & 0x3u) != 0;

  uint64_t fault_rsp = (uint64_t)(frame + 4);
  uint64_t fault_ss = 0;
  if (privilege_transition) {
    fault_rsp = frame[4];
    fault_ss = frame[5];
  }

  const bool has_selector = error_code != 0;
  const bool external = (error_code & 0x1u) != 0;
  const uint8_t table_bits = (error_code >> 1) & 0x3u;
  const uint16_t descriptor_index = (error_code >> 3) & 0x1fffu;

  uint16_t ds, es, fs, gs, ss;
  asm volatile ("mov %%ds, %0" : "=r"(ds));
  asm volatile ("mov %%es, %0" : "=r"(es));
  asm volatile ("mov %%fs, %0" : "=r"(fs));
  asm volatile ("mov %%gs, %0" : "=r"(gs));
  asm volatile ("mov %%ss, %0" : "=r"(ss));

  gpf_log("= General protection fault caught =\n");
  gpf_log("  cpu_state @ %p\n", cpu_state);

  gpf_log("  Error code: 0x%016lx (%sselector, external=%s, table=%s, index=0x%x)\n",
          error_code,
          has_selector ? "" : "no ",
          external ? "yes" : "no",
          gpf_table_name(table_bits),
          descriptor_index);

  gpf_log("  Fault RIP: 0x%016lx  CS:0x%04lx  RFLAGS:0x%016lx\n", fault_rip, fault_cs, fault_rflags);
  if (privilege_transition) {
    gpf_log("  Fault RSP: 0x%016lx  SS:0x%04lx\n", fault_rsp, fault_ss);
  } else {
    gpf_log("  Stack pointer at fault: 0x%016lx\n", fault_rsp);
  }
  gpf_log("  Segment registers: DS=0x%04x ES=0x%04x FS=0x%04x GS=0x%04x SS=0x%04x\n",
          ds, es, fs, gs, ss);

  gpf_log("  General purpose registers:\n");
  gpf_log("    RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n",
          cpu_state->rax, cpu_state->rbx, cpu_state->rcx, cpu_state->rdx);
  gpf_log("    RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n",
          cpu_state->rsi, cpu_state->rdi, cpu_state->rbp, fault_rsp);
  gpf_log("    R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n",
          cpu_state->r8, cpu_state->r9, cpu_state->r10, cpu_state->r11);
  gpf_log("    R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n",
          cpu_state->r12, cpu_state->r13, cpu_state->r14, cpu_state->r15);
  gpf_log("    Saved RFLAGS snapshot: 0x%016lx\n", cpu_state->rflags);

  halt();
}

void idt_pf_isr_handler(uint64_t error_code) {
  puts("= Page fault caught.\n");
  uint64_t faulting_address;
  int present    = (error_code & 0x01);
  int write      = (error_code & 0x02) >> 1;
  int user_mode  = (error_code & 0x04) >> 2;
  int reserved   = (error_code & 0x08) >> 3;
  // int fetch      = (error_code & 0x10) >> 4;
  // int protection = (error_code & 0x20) >> 5;
  // int shadow     = (error_code & 0x40) >> 6;
  int index      = (error_code & 0xff0) >> 4;

  asm volatile ("movq %%cr2, %0" : "=r" (faulting_address));

  serial_puts("Page fault caught trying to access address:\n");

  printf("- Page fault caught trying to access address %lx. Error code: %ld\n", faulting_address, error_code);
  printf("  Index %d\n", index);
  printf("  Present: %d, Write: %d, User Mode: %d, Reserved: %d\n", present, write, user_mode, reserved);
  serial_printf("  Present: %d, Write: %d, User Mode: %d, Reserved: %d\n", present, write, user_mode, reserved);

  halt();
}
