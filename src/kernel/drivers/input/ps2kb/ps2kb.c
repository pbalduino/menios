#include <errno.h>
#include <kernel/console.h>
#include <kernel/driver.h>
#include <kernel/drivers/input/ps2kb.h>
#include <kernel/drivers/input/ps2.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/input.h>
#include <kernel/serial.h>
#include <kernel/arch/x86_64/idt.h>
#include <menios/input.h>

#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <uacpi/status.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define KB_BUFFER_SIZE 64

#define PIC1_COMM 0x20
#define PIC1_DATA (PIC1_COMM + 1)

#define PIC2_COMM 0xa0
#define PIC2_DATA (PIC2_COMM + 1)

extern void ps2kb_isr_handler();

static volatile uint8_t key_buffer[KB_BUFFER_SIZE];
static volatile uint8_t buffer_head;
static volatile uint8_t buffer_tail;

static bool left_shift;
static bool right_shift;
static bool left_ctrl;
static bool right_ctrl;
static bool left_alt;
static bool right_alt;
static bool caps_lock;
static bool extended_code;
static bool pic_remapped;

static const char scancode_unshift[128] = {
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

static const char scancode_shift[128] = {
  [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
  [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
  [0x0C] = '_', [0x0D] = '+', [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E',
  [0x13] = 'R', [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
  [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}', [0x1E] = 'A',
  [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H',
  [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':', [0x28] = '"',
  [0x29] = '~', [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
  [0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
  [0x34] = '>', [0x35] = '?'
};

static inline bool buffer_empty(void) {
  return buffer_head == buffer_tail;
}

static void buffer_push(uint8_t ch) {
  uint8_t next = (buffer_head + 1) % KB_BUFFER_SIZE;
  if(next == buffer_tail) {
    buffer_tail = (buffer_tail + 1) % KB_BUFFER_SIZE;
  }
  key_buffer[buffer_head] = ch;
  buffer_head = next;
  stdin_enqueue_char(ch);
}

static uint8_t buffer_pop(void) {
  uint8_t value = 0;
  if(!buffer_empty()) {
    value = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KB_BUFFER_SIZE;
  }
  return value;
}

static bool is_alpha(char ch) {
  return (ch >= 'a' && ch <= 'z');
}

static char translate_scancode(uint8_t code) {
  char base = scancode_unshift[code];
  if(base == 0) {
    return 0;
  }

  bool shift = left_shift || right_shift;
  bool ctrl = left_ctrl || right_ctrl;

  if(ctrl) {
    char temp = base;
    if(temp >= 'A' && temp <= 'Z') {
      temp = (char)(temp - 'A' + 'a');
    }
    if(temp >= 'a' && temp <= 'z') {
      return (char)((temp - 'a') + 1);
    }
    if(base == '[') {
      return 0x1b;
    }
    if(base == '\\') {
      return 0x1c;
    }
    if(base == ']') {
      return 0x1d;
    }
    if(base == '^') {
      return 0x1e;
    }
    if(base == '_') {
      return 0x1f;
    }
  }

  if(is_alpha(base)) {
    bool upper = shift ^ caps_lock;
    if(upper) {
      base = (char)(base - ('a' - 'A'));
    }
    return base;
  }

  if(shift) {
    char shifted = scancode_shift[code];
    if(shifted != 0) {
      return shifted;
    }
  }

  if(caps_lock && (base == ' ')) {
    return base;
  }

  return base;
}

static inline void io_wait(void) {
  outb(0x80, 0);
}

static uint8_t current_modifiers(void) {
  uint8_t mods = 0;
  if(left_shift || right_shift) {
    mods |= MENIOS_KEY_MOD_SHIFT;
  }
  if(left_ctrl || right_ctrl) {
    mods |= MENIOS_KEY_MOD_CTRL;
  }
  if(left_alt || right_alt) {
    mods |= MENIOS_KEY_MOD_ALT;
  }
  if(caps_lock) {
    mods |= MENIOS_KEY_MOD_CAPS;
  }
  return mods;
}

static void emit_key_event(uint8_t scancode, bool pressed, bool extended, uint8_t ascii) {
  menios_key_event_t event;
  memset(&event, 0, sizeof(event));
  event.scancode = scancode;
  event.ascii = ascii;
  event.pressed = pressed ? 1u : 0u;
  event.extended = extended ? 1u : 0u;
  event.modifiers = current_modifiers();
  keyboard_event_push(&event);
}

static void pic_remap(void) {
  if(pic_remapped) {
    return;
  }

  uint8_t master_mask = inb(PIC1_DATA);
  uint8_t slave_mask  = inb(PIC2_DATA);

  outb(PIC1_COMM, 0x11); io_wait();
  outb(PIC2_COMM, 0x11); io_wait();

  outb(PIC1_DATA, 0x20); io_wait();
  outb(PIC2_DATA, 0x28); io_wait();

  outb(PIC1_DATA, 0x04); io_wait();
  outb(PIC2_DATA, 0x02); io_wait();

  outb(PIC1_DATA, 0x01); io_wait();
  outb(PIC2_DATA, 0x01); io_wait();

  outb(PIC1_DATA, master_mask); io_wait();
  outb(PIC2_DATA, slave_mask); io_wait();

  pic_remapped = true;
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

static void irq_eoi(void) {
  outb(PIC1_COMM, 0x20);
}

void ps2kb_handler() {
  uint8_t scancode = inb(PS2_DATA_PORT);
  serial_printf("ps2kb: scancode=0x%02x extended=%d\n", scancode, extended_code ? 1 : 0);

  if(scancode == 0xE0) {
    extended_code = true;
    irq_eoi();
    return;
  }

  if(scancode == 0xE1) {
    // Pause/Break sequence, ignore for now
    extended_code = false;
    irq_eoi();
    return;
  }

  if(scancode == 0xFA || scancode == 0xFE) {
    // ACK or RESEND - ignore
    irq_eoi();
    return;
  }

  bool release = (scancode & 0x80) != 0;
  uint8_t code = scancode & 0x7F;

  if(extended_code) {
    extended_code = false;
    if(code == 0x1D) { // Right Control
      right_ctrl = !release;
      emit_key_event(code, !release, true, 0);
      irq_eoi();
      return;
    }
    if(code == 0x38) { // Right Alt
      right_alt = !release;
      emit_key_event(code, !release, true, 0);
      irq_eoi();
      return;
    }
    if(!release) {
      switch(code) {
        case 0x48: // Up
          buffer_push('\x1b'); buffer_push('['); buffer_push('A'); break;
        case 0x50: // Down
          buffer_push('\x1b'); buffer_push('['); buffer_push('B'); break;
        case 0x4B: // Left
          buffer_push('\x1b'); buffer_push('['); buffer_push('D'); break;
        case 0x4D: // Right
          buffer_push('\x1b'); buffer_push('['); buffer_push('C'); break;
        case 0x53: // Delete
          buffer_push('\x1b'); buffer_push('['); buffer_push('3'); buffer_push('~'); break;
        default:
          break;
      }
    }
    emit_key_event(code, !release, true, 0);
    irq_eoi();
    return;
  }

  switch(code) {
    case 0x2A: // Left Shift
      left_shift = !release;
      emit_key_event(code, !release, false, 0);
      irq_eoi();
      return;
    case 0x36: // Right Shift
      right_shift = !release;
      emit_key_event(code, !release, false, 0);
      irq_eoi();
      return;
    case 0x1D: // Left Control
      left_ctrl = !release;
      emit_key_event(code, !release, false, 0);
      irq_eoi();
      return;
    case 0x38: // Left Alt
      left_alt = !release;
      emit_key_event(code, !release, false, 0);
      irq_eoi();
      return;
    case 0x3A: // Caps Lock
      if(!release) {
        caps_lock = !caps_lock;
      }
      emit_key_event(code, !release, false, 0);
      irq_eoi();
      return;
    default:
      break;
  }

  if(!release) {
    char ch = translate_scancode(code);
    serial_printf("ps2kb: translated '%c' (code=0x%02x)\n", ch ? ch : '?', code);
    if(ch != 0) {
      buffer_push((uint8_t)ch);
    }
    emit_key_event(code, true, false, (uint8_t)ch);
  } else {
    emit_key_event(code, false, false, 0);
  }

  irq_eoi();
}

static bool ps2_read_byte(uint8_t *out) {
  for(int timeout = 0; timeout < 100000; ++timeout) {
    if(inb(PS2_COMMAND_PORT) & 0x01) {
      *out = inb(PS2_DATA_PORT);
      return true;
    }
  }
  return false;
}

void ps2kb_start(void) {
  buffer_head = buffer_tail = 0;
  left_shift = right_shift = left_ctrl = right_ctrl = left_alt = right_alt = caps_lock = false;
  extended_code = false;

  pic_remap();

  struct acpi_fadt *fadt = NULL;
  uacpi_status status = uacpi_table_fadt(&fadt);
  if(uacpi_unlikely_error(status)) {
    serial_printf("ps2kb_start: failed to retrieve FADT (%s)\n", uacpi_status_to_string(status));
    return;
  }

  if((fadt->iapc_boot_arch & ACPI_IA_PC_8042) == 0) {
    serial_printf("ps2kb_start: system does not advertise an 8042 controller\n");
    return;
  }

  // Flush any pending data
  while(inb(PS2_COMMAND_PORT) & 0x01) {
    (void)inb(PS2_DATA_PORT);
  }

  ps2_wait_write();
  ps2_write_command(PS2_DISABLE_FIRST_PORT);

  ps2_wait_write();
  ps2_write_command(0x20); // Read controller configuration byte
  uint8_t config = 0;
  if(!ps2_read_byte(&config)) {
    serial_printf("ps2kb_start: timed out reading controller configuration\n");
    return;
  }

  config |= 0x01;   // Enable first port interrupt
  config &= ~(1 << 4); // Ensure first port clock enabled

  ps2_wait_write();
  ps2_write_command(PS2_WRITE_MODE);
  ps2_wait_write();
  ps2_write_data(config);

  ps2_wait_write();
  ps2_write_command(PS2_ENABLE_FIRST_PORT);

  ps2_wait_write();
  ps2_write_data(PS2_ENABLE_SCANNING);
  uint8_t response;
  if(ps2_read_byte(&response) && response != 0xFA) {
    serial_printf("ps2kb_start: unexpected response 0x%x enabling scanning\n", response);
  }

  uint8_t mask = inb(PIC1_DATA);
  mask &= ~(1 << 1);
  outb(PIC1_DATA, mask);

  serial_printf("ps2kb_start: PS/2 keyboard initialised\n");
}

void ps2kb_shutdown(void) {
  ps2_wait_write();
  ps2_write_command(PS2_DISABLE_FIRST_PORT);
}

static uint8_t ps2kb_read(void) {
  uint8_t ch = 0;
  disable_interrupts();
  ch = buffer_pop();
  enable_interrupts();
  return ch;
}

void ps2kb_write(void) {
}

int ps2kb_ioctl(void* device, unsigned long request, void* argp) {
  (void)device;
  (void)request;
  (void)argp;
  return -ENOTTY;
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

int kgetchar(void) {
  while(true) {
    uint8_t ch = ps2kb_read();
    if(ch != 0) {
      return (int)ch;
    }
    asm volatile("hlt");
  }
}

int getchar(void) {
  return kgetchar();
}
