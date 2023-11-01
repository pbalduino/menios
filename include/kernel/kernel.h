#ifndef _KERNEL_H
#define _KERNEL_H 1

#include <stdint.h>

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);

void cli();
void hcf() __attribute__((noreturn));
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void sti();

#endif
