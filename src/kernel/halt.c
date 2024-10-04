#include <stdio.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

void cli() {
  puts("- Stopping interruptions.\n");
  asm("cli");
}

void sti() {
  puts("- Resuming interruptions.\n");
  asm("sti");
}

// Halt and catch fire function.
void hcf() {
  cli();

  puts("System halted.\n");
  serial_log("System halted.");
  for (;;) {
    asm("hlt");
  }
}

// Writes a byte to a port
void outb(uint16_t port, uint8_t value) {
  asm volatile (
    "outb %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

// Reads a byte from a port
uint8_t inb(uint16_t port) {
  uint8_t result;
  asm volatile (
    "inb %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}

// Function to write a 16-bit value to a port
void outw(uint16_t port, uint16_t value) {
  asm volatile (
    "outw %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

// Function to read a 16-bit value from a port
uint16_t inw(uint16_t port) {
  uint16_t result;
  asm volatile (
    "inw %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}

// Function to write a 32-bit value to a port
void outl(uint16_t port, uint32_t value) {
  asm volatile (
    "outl %0, %1"
    :
    : "a"(value), "Nd"(port)
  );
}

// Function to read a 16-bit value from a port
uint32_t inl(uint16_t port) {
  uint32_t result;
  asm volatile (
    "inl %1, %0"
    : "=a"(result)
    : "Nd"(port)
  );
  return result;
}