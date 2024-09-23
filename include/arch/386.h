#ifndef MENIOS_INCLUDE_ARCH_386_H
#define MENIOS_INCLUDE_ARCH_386_H

#include <types.h>

uint32_t __force_order;

static inline uint8_t inb(int port) {
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline uint32_t inl(int port) {
	uint32_t data;
	asm volatile("inl %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline void insl(int port, void *addr, int cnt) {
	asm volatile("cld\n\trepne\n\tinsl"
		     : "=D" (addr), "=c" (cnt)
		     : "d" (port), "0" (addr), "1" (cnt)
		     : "memory", "cc");
}

static inline void outb(int port, uint8_t data) {
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
}

static inline void outl(int port, uint32_t data) {
	asm volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

static inline void cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp) {
	uint32_t eax, ebx, ecx, edx;
	asm volatile("cpuid"
		     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		     : "a" (info));
	if(eaxp)
		*eaxp = eax;
	if(ebxp)
		*ebxp = ebx;
	if(ecxp)
		*ecxp = ecx;
	if(edxp)
		*edxp = edx;
}

static inline uint32_t read_cr2() {
	uint32_t val;
	asm volatile("mov %%cr2,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline uint32_t read_esp() {
	uint32_t val;
	asm volatile("mov %%esp,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline uint32_t read_ebp() {
	uint32_t val;
	asm volatile("mov %%ebp,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline void io_wait() {
	asm volatile ("jmp 1f\n\t"
                "1:jmp 2f\n\t"
                "2:" );
}

static inline void pause_interrupts() {
	asm volatile("cli");
}

static inline void resume_interrupts() {
	asm volatile("sti");
}

#endif
