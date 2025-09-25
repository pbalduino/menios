#include <stdio.h>
#include <kernel/console.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

/**
 * Disables interrupts by executing the CLI (Clear Interrupt Flag) instruction.
 * This prevents the CPU from responding to maskable hardware interrupts.
 */
void disable_interrupts() {
  asm("cli");
}

/**
 * Enables interrupts by executing the STI (Set Interrupt Flag) instruction.
 * This allows the CPU to respond to maskable hardware interrupts.
 */
void enable_interrupts() {
  asm("sti");
}

/**
 * Halts the system by disabling interrupts and entering an infinite loop.
 * This is the "Halt and Catch Fire" function, typically used for unrecoverable errors.
 * 
 * @note Prints "System halted" to both standard output and serial log before halting
 */
void halt() {
  disable_interrupts();

  logk("System halted.\n");
  serial_log("System halted.");
  for(;;) {
    asm("hlt");
  }
}

/**
 * Writes an 8-bit value to an I/O port.
 * 
 * @param port   The I/O port address to write to (16-bit)
 * @param value  The byte value to write to the port
 * 
 * @note Uses inline assembly with the outb instruction
 */
void outb(uint16_t port, uint8_t value) {
  asm volatile (
    "outb %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

/**
 * Reads an 8-bit value from an I/O port.
 * 
 * @param port  The I/O port address to read from (16-bit)
 * @return      The byte value read from the port
 * 
 * @note Uses inline assembly with the inb instruction
 */
uint8_t inb(uint16_t port) {
  uint8_t result;
  asm volatile (
    "inb %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}

/**
 * Writes a 16-bit value to an I/O port.
 * 
 * @param port   The I/O port address to write to (16-bit)
 * @param value  The word value to write to the port
 * 
 * @note Uses inline assembly with the outw instruction
 */
void outw(uint16_t port, uint16_t value) {
  asm volatile (
    "outw %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

/**
 * Reads a 16-bit value from an I/O port.
 * 
 * @param port  The I/O port address to read from (16-bit)
 * @return      The word value read from the port
 * 
 * @note Uses inline assembly with the inw instruction
 */
uint16_t inw(uint16_t port) {
  uint16_t result;
  asm volatile (
    "inw %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}

/**
 * Writes a 32-bit value to an I/O port.
 * 
 * @param port   The I/O port address to write to (16-bit)
 * @param value  The double word value to write to the port
 * 
 * @note Uses inline assembly with the outl instruction
 */
void outl(uint16_t port, uint32_t value) {
  asm volatile (
    "outl %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

/**
 * Reads a 32-bit value from an I/O port.
 * 
 * @param port  The I/O port address to read from (16-bit)
 * @return      The double word value read from the port
 * 
 * @note Uses inline assembly with the inl instruction
 */
uint32_t inl(uint16_t port) {
  uint32_t result;
  asm volatile (
    "inl %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}

/**
 * Executes a no-operation instruction.
 * This instruction performs no operation but can be useful for timing
 * or preventing compiler optimizations.
 * 
 * @note Uses inline assembly with the nop instruction
 */
void noop() {
  __asm__("nop");
}
