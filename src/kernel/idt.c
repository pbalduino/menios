#include <kernel/console.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/vm_region.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

static idt_pointer_t idt_p; // __attribute__((aligned(8)));
static idt_entry_t idt[0x100]; // __attribute__((aligned(4096)));

static void idt_set_entry(int interruption, void* handler, uint8_t type_attr) {
  uintptr_t handler_address = (uintptr_t)handler;

  idt[interruption].base_low = (uint16_t)(handler_address & 0xFFFF);
  idt[interruption].selector = KERNEL_CODE_SEGMENT;
  idt[interruption].ist = 0;
  idt[interruption].type_attr = type_attr;
  idt[interruption].base_mid = (uint16_t)((handler_address >> 16) & 0xffff);
  idt[interruption].base_high = (uint32_t)((handler_address >> 32) & 0xffffffff);
  idt[interruption].reserved = 0;
}

static void exception_log(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  va_start(args, fmt);
  serial_vprintf(fmt, args);
  va_end(args);
}

static const char *gpf_table_name(idt_gpf_table_t table) {
  switch (table) {
    case IDT_GPF_TABLE_GDT: return "GDT";
    case IDT_GPF_TABLE_IDT: return "IDT";
    case IDT_GPF_TABLE_LDT: return "LDT";
    case IDT_GPF_TABLE_IDT_2: return "IDT";
    default: return "?";
  }
}

void idt_add_isr(int interruption, void* handler) {
  idt_set_entry(interruption, handler, 0x8e);
}

void idt_add_user_isr(int interruption, void* handler) {
  idt_set_entry(interruption, handler, 0xee);
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
  idt_add_isr(ISR_AHCI, &ahci_isr_handler);
  idt_add_user_isr(ISR_SYSCALL, &syscall_isr_handler);

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

  idt_gpf_error_info_t info;
  idt_decode_gpf(error_code, &info);

  uint16_t ds, es, fs, gs, ss;
  asm volatile ("mov %%ds, %0" : "=r"(ds));
  asm volatile ("mov %%es, %0" : "=r"(es));
  asm volatile ("mov %%fs, %0" : "=r"(fs));
  asm volatile ("mov %%gs, %0" : "=r"(gs));
  asm volatile ("mov %%ss, %0" : "=r"(ss));

  exception_log("= General protection fault caught =\n");
  exception_log("  cpu_state @ %p\n", cpu_state);

  exception_log("  Error code: 0x%016lx (selector=%s, external=%s, table=%s, index=0x%x)\n",
                error_code,
                info.has_selector ? "yes" : "no",
                info.external ? "yes" : "no",
                gpf_table_name(info.table),
                info.descriptor_index);

  exception_log("  Fault RIP: 0x%016lx  CS:0x%04lx  RFLAGS:0x%016lx\n", fault_rip, fault_cs, fault_rflags);
  if (privilege_transition) {
    exception_log("  Fault RSP: 0x%016lx  SS:0x%04lx\n", fault_rsp, fault_ss);
  } else {
    exception_log("  Stack pointer at fault: 0x%016lx\n", fault_rsp);
  }
  exception_log("  Segment registers: DS=0x%04x ES=0x%04x FS=0x%04x GS=0x%04x SS=0x%04x\n",
          ds, es, fs, gs, ss);

  exception_log("  General purpose registers:\n");
  exception_log("    RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n",
          cpu_state->rax, cpu_state->rbx, cpu_state->rcx, cpu_state->rdx);
  exception_log("    RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n",
          cpu_state->rsi, cpu_state->rdi, cpu_state->rbp, fault_rsp);
  exception_log("    R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n",
          cpu_state->r8, cpu_state->r9, cpu_state->r10, cpu_state->r11);
  exception_log("    R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n",
          cpu_state->r12, cpu_state->r13, cpu_state->r14, cpu_state->r15);
  exception_log("    Saved RFLAGS snapshot: 0x%016lx\n", cpu_state->rflags);

  halt();
}

void idt_pf_isr_handler(idt_exception_p cpu_state) {
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

  uint16_t ds, es, fs, gs, ss;
  asm volatile ("mov %%ds, %0" : "=r"(ds));
  asm volatile ("mov %%es, %0" : "=r"(es));
  asm volatile ("mov %%fs, %0" : "=r"(fs));
  asm volatile ("mov %%gs, %0" : "=r"(gs));
  asm volatile ("mov %%ss, %0" : "=r"(ss));

  uint64_t cr2;
  asm volatile ("mov %%cr2, %0" : "=r"(cr2));

  idt_pf_error_info_t info;
  idt_decode_page_fault(error_code, &info);

  bool handled = false;
  if(info.user) {
    handled = vm_region_handle_page_fault(current, cr2, info.present, info.write, info.user);
  }

  if(handled) {
    return;
  }

  exception_log("= Page fault caught =\n");
  exception_log("  cpu_state @ %p\n", cpu_state);
  exception_log("  Faulting address: 0x%016lx\n", cr2);
  exception_log("  Error code: 0x%016lx (present=%s, write=%s, user=%s, reserved=%s, instruction=%s, protection=%s, shadow=%s, sgx=%s)\n",
                error_code,
                info.present ? "yes" : "no",
                info.write ? "yes" : "no",
                info.user ? "yes" : "no",
                info.reserved ? "yes" : "no",
                info.instruction_fetch ? "yes" : "no",
                info.protection_key ? "yes" : "no",
                info.shadow_stack ? "yes" : "no",
                info.sgx_violation ? "yes" : "no");

  exception_log("  Fault RIP: 0x%016lx  CS:0x%04lx  RFLAGS:0x%016lx\n", fault_rip, fault_cs, fault_rflags);
  if (privilege_transition) {
    exception_log("  Fault RSP: 0x%016lx  SS:0x%04lx\n", fault_rsp, fault_ss);
  } else {
    exception_log("  Stack pointer at fault: 0x%016lx\n", fault_rsp);
  }
  exception_log("  Segment registers: DS=0x%04x ES=0x%04x FS=0x%04x GS=0x%04x SS=0x%04x\n",
                ds, es, fs, gs, ss);

  exception_log("  General purpose registers:\n");
  exception_log("    RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx\n",
                cpu_state->rax, cpu_state->rbx, cpu_state->rcx, cpu_state->rdx);
  exception_log("    RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx\n",
                cpu_state->rsi, cpu_state->rdi, cpu_state->rbp, fault_rsp);
  exception_log("    R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx\n",
                cpu_state->r8, cpu_state->r9, cpu_state->r10, cpu_state->r11);
  exception_log("    R12=%016lx R13=%016lx R14=%016lx R15=%016lx\n",
                cpu_state->r12, cpu_state->r13, cpu_state->r14, cpu_state->r15);
  exception_log("    Saved RFLAGS snapshot: 0x%016lx\n", cpu_state->rflags);

  halt();
}
