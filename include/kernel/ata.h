#ifndef MENIOS_INCLUDE_KERNEL_ATA_H
#define MENIOS_INCLUDE_KERNEL_ATA_H

#include <types.h>

#define SECTSIZE 512

#define ATA_BSY		0x80
#define ATA_DRDY	0x40
#define ATA_DF		0x20
#define ATA_ERR		0x01

#define ATA_MASTER 0x00
#define ATA_NOT_MASTER 0x01 // you know, the other ATA that is not master

typedef struct {
  uint16_t primary_channel;
  uint16_t secondary_channel;
} ata_t;

ata_t ata;

int ata_read(uint32_t secno, void *dst, size_t nsecs);

#endif
