#ifndef MENIOS_INCLUDE_KERNEL_PIC_H
#define MENIOS_INCLUDE_KERNEL_PIC_H

#define PIC1_COMM 0x20
#define PIC1_DATA (PIC1_COMM + 1)

#define PIC2_COMM 0xa0
#define PIC2_DATA (PIC2_COMM + 1)

#define ICW1_INIT	0x10		/* Initialization - required! */
#define ICW1_ICW4 0x01
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */

#endif
