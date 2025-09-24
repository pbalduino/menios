#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/driver/ps2kb.h>
#include <kernel/driver/ps2.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/idt.h>
#include <stdio.h>

#define KB_BUFFER_SIZE 32

#define PIC1_COMM 0x20
#define PIC1_DATA (PIC1_COMM + 1)

#define PIC2_COMM 0xa0
#define PIC2_DATA (PIC2_COMM + 1)

extern void ps2kb_isr_handler();

static char buffer[KB_BUFFER_SIZE];
static int read = 0;
static int write = 0;


uint8_t keyboard_map[128] = {
  0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
  '9', '0', '-', '=', '\b',     /* Backspace */
  '\t',                 /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
  0,                  /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
  '\'', '`',   0,                /* Left shift */
  '\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
  'm', ',', '.', '/',   0,                              /* Right shift */
  '*',
  0,  /* Alt */
  ' ',  /* Space bar */
  0,  /* Caps lock */
  0,  /* 59 - F1 key ... > */
  0,   0,   0,   0,   0,   0,   0,   0,
  0,  /* < ... F10 */
  0,  /* 69 - Num lock*/
  0,  /* Scroll Lock */
  0,  /* Home key */
  0,  /* Up Arrow */
  0,  /* Page Up */
  '-',
  0,  /* Left Arrow */
  0,
  0,  /* Right Arrow */
  '+',
  0,  /* 79 - End key*/
  0,  /* Down Arrow */
  0,  /* Page Down */
  0,  /* Insert Key */
  0,  /* Delete Key */
  0,   0,   0,
  0,  /* F11 Key */
  0,  /* F12 Key */
  0,  /* All other keys are undefined */
};

uint8_t ps2_read_data() {
  return inb(PS2_DATA_PORT);
}

void ps2_write_data(uint8_t data) {
  outb(PS2_DATA_PORT, data);
}

void ps2_write_command(uint8_t command) {
  outb(PS2_COMMAND_PORT, command);
}

void ps2_wait_write() {
  while (inb(PS2_COMMAND_PORT) & 0x02);
}

void irq_eoi() {
	// OCW2: rse00xxx
	//   r: rotate
	//   s: specific
	//   e: end-of-interrupt
	// xxx: specific interrupt line
	outb(PIC1_COMM, 0x20);
	outb(PIC2_COMM, 0x20);
}

void ps2kb_handler() {
  serial_line("");
  serial_printf("ps2kb_handler: Keyboard interrupt\n");

  // uint8_t scancode = ps2_read_data();

  // // Check if it’s a key press (not a release code)
  // if (!(scancode & 0x80)) {
  //   // char key = scancode_to_ascii[scancode];
  //   printf("ps2kb_isr: Key pressed: %c\n", scancode);
  //   buffer[write++] = scancode;
  //   write %= KB_BUFFER_SIZE;
  // }
  puts("&");

  irq_eoi();
}

void ps2kb_start(void) {
  serial_line("");
  serial_printf("ps2kb_start: Initializing PS/2 keyboard\n");
  errk("    Ignoring PS/2 keyboard.\n");

  // while(inb(0x64) & 0x02) {
  //   puts("-");
  // };

  // outb(0x64, 0xad); // Disable PS/2 keyboard
  // while (inb(0x64) & 0x01) {
  //   puts("*");
  // };

  // uint8_t mask = inb(PIC1_DATA);
  // mask &= ~(1 << 1); // Clear bit 1
  // outb(PIC1_DATA, mask);
  // // // idt_add_isr(ISR_KEYBOARD, &ps2kb_isr_handler);
  // // // idt_refresh();

  // outb(0x64, 0xae); // Enable PS/2 keyboard
  // // outb(0x60, 0xf4); // Send enable scanning command to the keyboard

  // uint8_t response = inb(0x60);
  // if (response != 0xfa) {
  //   serial_printf("ps2kb_start: Failed to enable scanning (response: 0x%x)\n", response);
  //   return;
  // }

  // serial_printf("ps2kb_start: PS/2 keyboard initialized successfully: %x\n", response);
}

void ps2kb_shutdown(void) {
  serial_line("");
}

uint8_t ps2kb_read(void) {
  uint8_t scancode = ps2_read_data();
  if(scancode != 0xfa) {
    serial_printf("ps2kb_read: scancode: %x\n", scancode);
    return 0;
  }

  if (!(scancode & 0x80)) {
    serial_printf("ps2kb_read: Key pressed: %d\n", scancode);
  }
  return scancode;
}

void ps2kb_write(void) {
  serial_line("");
}

void ps2kb_ioctl(void) {
  serial_line("");
}

static struct driver_t ps2kb_driver = {
  .hid = "PNP0303",
  .ioctl = ps2kb_ioctl,
  .name = "Standard PS/2 keyboard",
  .read = ps2kb_read,
  .shutdown = ps2kb_shutdown,
  .start = ps2kb_start,
  .write = ps2kb_write,
};

void ps2kb_init(void) {
  serial_printf("ps2kb_init: Registering driver '%s' for HID '%s'\n", ps2kb_driver.name, ps2kb_driver.hid);
  driver_register(&ps2kb_driver);
}

int getchar() {
  int scancode;

  while(1) {
    while(!(inb(0x64) & 0x01)) {};

    scancode = inb(0x60);

    irq_eoi();

    if(scancode & 0x80) {
      continue;
    } else {
      if(keyboard_map[scancode]) {
        break;
      } else {
        serial_printf("Key not found for scancode %x\n", scancode);
      }
    }
  }

  return keyboard_map[scancode];
}
