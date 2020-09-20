#ifndef MENIOS_INCLUDE_KERNEL_ATA_H
#define MENIOS_INCLUDE_KERNEL_ATA_H

#include <types.h>

#define SECTSIZE 512

// Status
#define ATA_SR_BSY  0x80    // Busy
#define ATA_SR_DRDY 0x40    // Drive ready
#define ATA_SR_DF   0x20    // Drive write fault
#define ATA_SR_DSC  0x10    // Drive seek complete
#define ATA_SR_DRQ  0x08    // Data request ready
#define ATA_SR_CORR 0x04    // Corrected data
#define ATA_SR_IDX  0x02    // Index
#define ATA_SR_ERR  0x01    // Error

// Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xc8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xca
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xe7
#define ATA_CMD_CACHE_FLUSH_EXT   0xea
#define ATA_CMD_PACKET            0xa0
#define ATA_CMD_IDENTIFY_PACKET   0xa1
#define ATA_CMD_IDENTIFY          0xec

// Task file
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0a
#define ATA_REG_LBA5       0x0b
#define ATA_REG_CONTROL    0x0c
#define ATA_REG_ALTSTATUS  0x0c
#define ATA_REG_DEVADDRESS 0x0d
/*
Although they were in common use, the terms "master" and "slave" do not appear anymore in current versions of the ATA specifications, or any current documentation. Since ATA-2 the two devices are referred to as "Device 0" and "Device 1", respectively. This is more appropriate since the two devices have always operated, since the earliest ATA specification, as equal peers on the cable, with neither having control or priority over the other.

It is a common myth that the controller on the device 0 drive assumes control over the device 1 drive, or that the device 0 drive may claim priority of communication over the other device on the same ATA interface. In fact, the drivers in the host operating system perform the necessary arbitration and serialization, and each drive's onboard controller operates independently of the other.

While it may have remained in colloquial use, the PC industry has not used ATA master/slave terminology in many years.

SOURCE: https://en.wikipedia.org/wiki/Parallel_ATA#Master_and_slave_clarification
*/
#define CHANNEL_PRIMARY 0x00
#define CHANNEL_SECONDARY 0x02

#define DEVICE_0 0x00 // formerly known as Master
#define DEVICE_1 0x01

#define DISK0 CHANNEL_PRIMARY | DEVICE_0
#define DISK1 CHANNEL_PRIMARY | DEVICE_1
#define DISK2 CHANNEL_SECONDARY | DEVICE_0
#define DISK3 CHANNEL_SECONDARY | DEVICE_1

#define HDA DISK0
#define HDB DISK1
#define HDC DISK2
#define HDD DISK3

typedef struct {
  uint16_t primary_channel;
  uint16_t primary_control;
  uint16_t secondary_channel;
  uint16_t secondary_control;
} ata_t;

int ata_read(uint8_t diskno, uint32_t secno, void *dst, size_t nsecs);
ata_t* init_ata();

#endif
