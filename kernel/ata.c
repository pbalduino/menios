#include <assert.h>
#include <x86.h>
#include <kernel/ata.h>

uint8_t diskno;

static int ata_wait_ready(bool check_error, uint16_t io_base) {
	int r;

	while (1) {
		r = inb(io_base + 7);

		if (r == 0xff) {
			panic("floating bus (no ATA drives)\n");
    }

 		if ((r & (ATA_BSY | ATA_DRDY)) == ATA_DRDY) {
			break;
    }
	}

	if (check_error && (r & (ATA_DF | ATA_ERR)) != 0) {
		return -1;
  }

	return 0;
}

int ata_read(uint32_t secno, void *dst, size_t nsecs) {
	int r;
  uint16_t io_base = diskno == 0 ? ata.primary_channel : ata.secondary_channel;

	assert(nsecs <= 256);

	ata_wait_ready(0, io_base);

	outb(io_base + 2, nsecs);
	outb(io_base + 3, secno & 0xff);
	outb(io_base + 4, (secno >> 8) & 0xff);
	outb(io_base + 5, (secno >> 16) & 0xff);
	outb(io_base + 6, 0xe0 | ((diskno & 1) << 4) | ((secno >> 24) & 0x0f));
	outb(io_base + 7, 0x20);	// CMD 0x20 means read sector

	for (; nsecs > 0; nsecs--, dst += SECTSIZE) {
		if ((r = ata_wait_ready(1, io_base)) < 0) {
			return r;
    }
		insl(io_base, dst, SECTSIZE / 4);
	}

	return 0;
}
