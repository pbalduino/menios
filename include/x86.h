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

// struct RSDPDescriptor {
//   char Signature[8];
//   uint8_t Checksum;
//   char OEMID[6];
//   uint8_t Revision;
//   uint32_t RsdtAddress;
// } __attribute__ ((packed));
//
// struct RSDPDescriptor20 {
//   RSDPDescriptor firstPart;
//
//   uint32_t Length;
//   uint64_t XsdtAddress;
//   uint8_t ExtendedChecksum;
//   uint8_t reserved[3];
// } __attribute__ ((packed));

#endif
