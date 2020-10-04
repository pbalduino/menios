#include <assert.h>
#include <x86.h>
#include <kernel/ata.h>
#include <kernel/pci.h>

static ata_t ata;

static int ata_wait_ready(bool check_error, uint16_t io_base, uint8_t devno) {
	int r = -1;

  printf("Reading DEVICE_%x - %x", (devno & 1), inb(io_base + ATA_REG_HDDEVSEL));

  printf(" comm: %x ", 0xe0 | (devno & 1) << 4);
  outb(io_base + ATA_REG_HDDEVSEL, 0xe0 | (devno & 1) << 4);

  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);

  outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  // printf("  inb %x ", (devno & 1), inb(io_base + ATA_REG_HDDEVSEL));
  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);
  inb(io_base + ATA_REG_STATUS);

  printf("  status: %x\n", inb(io_base + ATA_REG_STATUS));


  while(inb(io_base + ATA_REG_STATUS) & ATA_SR_BSY) {
    printf(".");
  };
  printf("\n");

	while(1) {
		r = inb(io_base + ATA_REG_STATUS);

    if(!r) {
      return -2;
    }

		if(r == 0xff) {
			panic("floating bus (no ATA drives)\n");
    }

 		if((r & (ATA_SR_BSY | ATA_SR_DRDY)) == ATA_SR_DRDY) {
			return 0;
    }
	}

	if(check_error && (r & (ATA_SR_DF | ATA_SR_ERR)) != 0) {
		return -1;
  }

	return -2;
}

int ata_read(uint8_t diskno, uint32_t secno, void *dst, size_t nsecs) {
	int r;
  uint16_t io_base = (diskno & 2) ? ata.secondary_channel : ata.primary_channel;
  // uint16_t io_ctrl = (diskno & 2) ? ata.secondary_control : ata.primary_control;
  uint8_t devno = diskno & 1;

  printf("Reading DEVICE_%x - %d\n", devno, __LINE__);

	assert(nsecs <= 256);

	int err = ata_wait_ready(0, io_base, devno);

  if(err) {
    printf("returning err %x\n", err);
    return err;
  }

	outb(io_base + 2, nsecs);
	outb(io_base + 3, secno & 0xff);
	outb(io_base + 4, (secno >> 8) & 0xff);
	outb(io_base + 5, (secno >> 16) & 0xff);
	outb(io_base + ATA_REG_HDDEVSEL, 0xe0 | (devno << 4) | ((secno >> 24) & 0x0f));
	outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

	for (; nsecs > 0; nsecs--, dst += SECTSIZE) {
		if((r = ata_wait_ready(1, io_base, devno)) < 0) {
			return r;
    }
		insl(io_base, dst, SECTSIZE / 4);
	}

	return 0;
}

ata_t* init_ata(pci_device_t** pci_devices) {
	int p = 0;
  for(p = 0; p < PCI_MAX_DEVICES; p++) {
    if(pci_devices[p]->class == 1 &&
       pci_devices[p]->subclass == 1 &&
       (pci_devices[p]->progif == 0x80 ||
         pci_devices[p]->progif == 0x8a)) {

      if(pci_devices[p]->bar0 == 0 || pci_devices[p]->bar0 == 1) {
        ata.primary_channel = 0x1f0;
        ata.primary_control = 0x3f6;
      }
      if(pci_devices[p]->bar2 == 0 || pci_devices[p]->bar2 == 1) {
        ata.secondary_channel = 0x170;
        ata.secondary_control = 0x376;
      }
      return &ata;
    }
  }

	return (ata_t*)NULL;
}
// int ata_write(uint8_t diskno, uint32_t secno, void *dst, size_t nsecs) {
//
// }
