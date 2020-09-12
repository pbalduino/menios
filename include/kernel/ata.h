#ifndef MENIOS_INCLUDE_KERNEL_ATA_H
#define MENIOS_INCLUDE_KERNEL_ATA_H

#include <types.h>

#define SECTSIZE 512

#define ATA_BSY		0x80
#define ATA_DRDY	0x40
#define ATA_DF		0x20
#define ATA_ERR		0x01

/*
Although they were in common use, the terms "master" and "slave" do not appear anymore in current versions of the ATA specifications, or any current documentation. Since ATA-2 the two devices are referred to as "Device 0" and "Device 1", respectively. This is more appropriate since the two devices have always operated, since the earliest ATA specification, as equal peers on the cable, with neither having control or priority over the other.

It is a common myth that the controller on the device 0 drive assumes control over the device 1 drive, or that the device 0 drive may claim priority of communication over the other device on the same ATA interface. In fact, the drivers in the host operating system perform the necessary arbitration and serialization, and each drive's onboard controller operates independently of the other.

While it may have remained in colloquial use, the PC industry has not used ATA master/slave terminology in many years.

SOURCE: https://en.wikipedia.org/wiki/Parallel_ATA#Master_and_slave_clarification
*/
#define DEVICE_0 0x00 // formerly known as Master
#define DEVICE_1 0x01

typedef struct {
  uint16_t primary_channel;
  uint16_t secondary_channel;
} ata_t;

ata_t ata;

int ata_read(uint32_t secno, void *dst, size_t nsecs);

#endif
