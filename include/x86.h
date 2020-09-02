#ifndef MENIOS_INCLUDE_X86_H
#define MENIOS_INCLUDE_X86_H

#include <types.h>

static inline uint8_t
inb(int port) {
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline void
outb(int port, uint8_t data) {
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
}

static inline void
cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp)
{
	uint32_t eax, ebx, ecx, edx;
	asm volatile("cpuid"
		     : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		     : "a" (info));
	if (eaxp)
		*eaxp = eax;
	if (ebxp)
		*ebxp = ebx;
	if (ecxp)
		*ecxp = ecx;
	if (edxp)
		*edxp = edx;
}

#endif
