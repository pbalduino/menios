#include <assert.h>
#include <stdio.h>
#include <types.h>
#include <x86.h>
#include <kernel/pci.h>
#include <kernel/storage.h>

#define SECTSIZE 512

#define IDE_BSY		0x80
#define IDE_DRDY	0x40
#define IDE_DF		0x20
#define IDE_ERR		0x01

uint16_t ide_port;
ide_t ide;
uint8_t diskno;

static int detect_storage(ide_t* ide) {
  int p = 0;
  for(p = 0; p < PCI_MAX_DEVICES; p++) {
    if(pci_devices[p].class == 1 &&
       pci_devices[p].subclass == 1 &&
       (pci_devices[p].progif == 0x80 ||
         pci_devices[p].progif == 0x8a)) {

      if(pci_devices[p].bar0 == 0 || pci_devices[p].bar0 == 1) {
        ide->primary_channel = 0x1f0;
      }
      if(pci_devices[p].bar2 == 0 || pci_devices[p].bar2 == 1) {
        ide->secondary_channel = 0x170;
      }
      return 1;
    }
  }
  return 0;
}

static int ide_wait_ready(bool check_error, uint16_t io_base) {
	int r;

	while (1) {
		r = inb(io_base + 7);

		if (r == 0xff) {
			panic("floating bus (no IDE drives)\n");
    }

 		if ((r & (IDE_BSY | IDE_DRDY)) == IDE_DRDY) {
			break;
    }
	}

	if (check_error && (r & (IDE_DF|IDE_ERR)) != 0) {
		return -1;
  }

	return 0;
}

int ide_read(uint32_t secno, void *dst, size_t nsecs) {
	int r;
  uint16_t io_base = diskno == 0 ? ide.primary_channel : ide.secondary_channel;

	assert(nsecs <= 256);

	ide_wait_ready(0, io_base);

	outb(io_base + 2, nsecs);
	outb(io_base + 3, secno & 0xff);
	outb(io_base + 4, (secno >> 8) & 0xff);
	outb(io_base + 5, (secno >> 16) & 0xff);
	outb(io_base + 6, 0xe0 | ((diskno & 1) << 4) | ((secno >> 24) & 0x0f));
	outb(io_base + 7, 0x20);	// CMD 0x20 means read sector

	for (; nsecs > 0; nsecs--, dst += SECTSIZE) {
		if ((r = ide_wait_ready(1, io_base)) < 0) {
			return r;
    }
		insl(io_base, dst, SECTSIZE / 4);
	}

	return 0;
}

void init_storage() {
  assert(is_pci_ready);

  assert(detect_storage(&ide));

  uint8_t sector_data[512];

  ide_read(0, &sector_data, 1);

  assert(sector_data[510] == 0x55 && sector_data[511] == 0xaa);

  printf("Primary IDE storage is ready\n\n");
}
