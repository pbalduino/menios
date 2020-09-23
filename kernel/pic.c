#include <kernel/pic.h>
#include <stdio.h>
#include <x86.h>

void init_pic() {
	printf("  Masking interrupts\n");

  // ICW1
  outb(PIC1_COMM, ICW1_INIT | ICW1_ICW4);
  io_wait();

  outb(PIC2_COMM, ICW1_INIT | ICW1_ICW4);
  io_wait();

  // Rest of Initialization command words get sent to data port
  // Map PIC1 to interrupts 32 - 39
  outb(PIC1_DATA, 0x20);
  io_wait();
  // Map PIC2 to interrupts 40 - 47
  outb(PIC2_DATA, 0x28);
  io_wait();

	// ICW3 - setup cascading
	outb(PIC1_DATA, 0x00);
	io_wait();

	outb(PIC2_DATA, 0x00);
	io_wait();

	/* ICW4 - environment info */
	outb(PIC1_DATA, 0x01);
	io_wait();

	outb(PIC2_DATA, 0x01);
	io_wait();

  // Each bit represents if IR is connected to a slave.  We want to remap IR 2
  // to the slave so we want the third bit to be 1.
  outb(PIC1_DATA, 0x04);
  io_wait();
  // Came from Slave ID table in 8259A manual
  outb(PIC2_DATA, 0x02);
  io_wait();

  // ICW4
  outb(PIC1_DATA, ICW4_8086);
  io_wait();

  outb(PIC2_DATA, ICW4_8086);
  io_wait();

	/* mask interrupts */
	outb(PIC1_DATA, 0xff);
	io_wait();
	outb(PIC2_DATA, 0xff);
  io_wait();
}
