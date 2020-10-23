#include <kernel/console.h>
#include <kernel/interrupts.h>
#include <kernel/irq.h>
#include <kernel/keyboard.h>
#include <arch.h>
#include <assert.h>
#include <string.h>
#include <types.h>

extern void irq_keyboard();
extern idt_entry_t idt[256];

#define NO		0

#define SHIFT		(1<<0)
#define CTL		(1<<1)
#define ALT		(1<<2)

#define CAPSLOCK	(1<<3)
#define NUMLOCK		(1<<4)
#define SCROLLLOCK	(1<<5)

#define E0ESC		(1<<6)

#define CONSBUFSIZE 512

#define C(x) (x - '@')

static struct {
	uint8_t buf[CONSBUFSIZE];
	uint32_t rpos;
	uint32_t wpos;
} cons;

static uint8_t shiftcode[256] = {
	[0x1D] = CTL,
	[0x2A] = SHIFT,
	[0x36] = SHIFT,
	[0x38] = ALT,
	[0x9D] = CTL,
	[0xB8] = ALT
};

static uint8_t togglecode[256] = {
	[0x3A] = CAPSLOCK,
	[0x45] = NUMLOCK,
	[0x46] = SCROLLLOCK
};

uint8_t charcode[4][128];

static int get_data_from_keyboard() {
	int c;
	uint8_t data;
	static uint32_t shift;

  data = inb(KBDATAP);

	if (data == 0xe0) {
		// E0 escape character
		shift |= E0ESC;
		return 0;
	} else if (data & 0x80) {
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	} else if (shift & E0ESC) {
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];

	c = charcode[shift & (CTL | SHIFT)][data];
	// printf("<[%x][%x]:%x>", shift & (CTL | SHIFT), data, c);
	if (shift & CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	return c;
}

static void add_to_console_buffer(char c) {
	cons.buf[cons.wpos++] = c;
	if (cons.wpos == CONSBUFSIZE) {
		cons.wpos = 0;
  }
}

static void clean_buffer() {
  uint8_t stat;
  int c;

  while ((stat = inb(KBSTATP)) & KBS_DIB) {
    if ((c = get_data_from_keyboard())) {
	    add_to_console_buffer(c);
    }
  }
}

// return the next input character from the console, or 0 if none waiting
int getchar() {
	int c;

	// poll for any pending input characters,
	// so that this function works even when interrupts are disabled
	// (e.g., when called from the kernel monitor).
	// clean_buffer();

	// grab the next character from the input buffer.
	if (cons.rpos != cons.wpos) {
		c = cons.buf[cons.rpos++];
		if (cons.rpos == CONSBUFSIZE)
			cons.rpos = 0;
		return c;
	}
	return 0;
}

void init_keyboard_layout() {
	// FIXME: ugh.

	// shiftcode
	shiftcode[0x1D] = CTL;
	shiftcode[0x2A] = SHIFT;
	shiftcode[0x36] = SHIFT;
	shiftcode[0x38] = ALT;
	shiftcode[0x9D] = CTL;
	shiftcode[0xB8] = ALT;

	// togglecode
	togglecode[0x3A] = CAPSLOCK;
	togglecode[0x45] = NUMLOCK;
	togglecode[0x46] = SCROLLLOCK;

	// normalmap
	charcode[0][0x01] = 0x1B;
	charcode[0][0x02] = '1';
	charcode[0][0x03] = '2';
	charcode[0][0x04] = '3';
	charcode[0][0x05] = '4';
	charcode[0][0x06] = '5';
	charcode[0][0x07] = '6';
	charcode[0][0x08] = '7';
	charcode[0][0x09] = '8';
	charcode[0][0x0a] = '9';
	charcode[0][0x0b] = '0';
	charcode[0][0x0c] = '-';
	charcode[0][0x0d] = '=';
	charcode[0][0x0e] = '\b';
	charcode[0][0x0f] = '\t';
	charcode[0][0x10] = 'q';
	charcode[0][0x11] = 'w';
	charcode[0][0x12] = 'e';
	charcode[0][0x13] = 'r';
	charcode[0][0x14] = 't';
	charcode[0][0x15] = 'y';
	charcode[0][0x16] = 'u';
	charcode[0][0x17] = 'i';
	charcode[0][0x18] = 'o';
	charcode[0][0x19] = 'p';
	charcode[0][0x1a] = '[';
	charcode[0][0x1b] = ']';
	charcode[0][0x1c] = '\n';
	charcode[0][0x1e] = 'a';
	charcode[0][0x1f] = 's';
	charcode[0][0x20] = 'd';
	charcode[0][0x21] = 'f';
	charcode[0][0x22] = 'g';
	charcode[0][0x23] = 'h';
	charcode[0][0x24] = 'j';
	charcode[0][0x25] = 'k';
	charcode[0][0x26] = 'l';
	charcode[0][0x27] = ';';
	charcode[0][0x28] = '\'';
	charcode[0][0x29] = '`';
	charcode[0][0x2b] = '\\';
	charcode[0][0x2c] = 'z';
	charcode[0][0x2d] = 'x';
	charcode[0][0x2e] = 'c';
	charcode[0][0x2f] = 'v';
	charcode[0][0x30] = 'b';
	charcode[0][0x31] = 'n';
	charcode[0][0x32] = 'm';
	charcode[0][0x33] = ',';
	charcode[0][0x34] = '.';
	charcode[0][0x35] = '/';
	charcode[0][0x37] = '*';
	charcode[0][0x39] = ' ';
	charcode[0][0x47] = '7';
	charcode[0][0x48] = '8';
	charcode[0][0x49] = '9';
	charcode[0][0x4a] = '-';
	charcode[0][0x4b] = '4';
	charcode[0][0x4c] = '5';
	charcode[0][0x4d] = '6';
	charcode[0][0x4e] = '+';
	charcode[0][0x4f] = '1';
	charcode[0][0x50] = '2';
	charcode[0][0x51] = '3';
	charcode[0][0x52] = '0';
	charcode[0][0x53] = '.';
	charcode[0][0xC7] = KEY_HOME;
	charcode[0][0x9C] = '\n';
	charcode[0][0xB5] = '/';
	charcode[0][0xC8] = KEY_UP;
	charcode[0][0xC9] = KEY_PGUP;
	charcode[0][0xCB] = KEY_LF;
	charcode[0][0xCD] = KEY_RT;
	charcode[0][0xCF] = KEY_END;
	charcode[0][0xD0] = KEY_DN;
	charcode[0][0xD2] = KEY_INS;
	charcode[0][0xD3] = KEY_DEL;

	charcode[1][0x01] = 033;
	charcode[1][0x02] = '!';
	charcode[1][0x03] = '@';
	charcode[1][0x04] = '#';
	charcode[1][0x05] = '$';
	charcode[1][0x06] = '%';
	charcode[1][0x07] = '^';
	charcode[1][0x08] = '&';
	charcode[1][0x09] = '*';
	charcode[1][0x0a] = '(';
	charcode[1][0x0b] = ')';
	charcode[1][0x0c] = '_';
	charcode[1][0x0d] = '+';
	charcode[1][0x0e] = '\b';
	charcode[1][0x0f] = '\t';
	charcode[1][0x10] = 'Q';
	charcode[1][0x11] = 'W';
	charcode[1][0x12] = 'E';
	charcode[1][0x13] = 'R';
	charcode[1][0x14] = 'T';
	charcode[1][0x15] = 'Y';
	charcode[1][0x16] = 'U';
	charcode[1][0x17] = 'I';
	charcode[1][0x18] = 'O';
	charcode[1][0x19] = 'P';
	charcode[1][0x1a] = '{';
	charcode[1][0x1b] = '}';
	charcode[1][0x1c] = '\n';
	charcode[1][0x1e] = 'A';
	charcode[1][0x1f] = 'S';
	charcode[1][0x20] = 'D';
	charcode[1][0x21] = 'F';
	charcode[1][0x22] = 'G';
	charcode[1][0x23] = 'H';
	charcode[1][0x24] = 'J';
	charcode[1][0x25] = 'K';
	charcode[1][0x26] = 'L';
	charcode[1][0x27] = ':';
	charcode[1][0x28] = '"';
	charcode[1][0x29] = '~';
	charcode[1][0x2b] = '|';
	charcode[1][0x2c] = 'Z';
	charcode[1][0x2d] = 'X';
	charcode[1][0x2e] = 'C';
	charcode[1][0x2f] = 'V';
	charcode[1][0x30] = 'B';
	charcode[1][0x31] = 'N';
	charcode[1][0x32] = 'M';
	charcode[1][0x33] = '<';
	charcode[1][0x34] = '>';
	charcode[1][0x35] = '?';
	charcode[1][0x37] = '*';
	charcode[1][0x39] = ' ';
	charcode[1][0x47] = '7';
	charcode[1][0x48] = '8';
	charcode[1][0x49] = '9';
	charcode[1][0x4a] = '-';
	charcode[1][0x4b] = '4';
	charcode[1][0x4c] = '5';
	charcode[1][0x4d] = '6';
	charcode[1][0x4e] = '+';
	charcode[1][0x4f] = '1';
	charcode[1][0x50] = '2';
	charcode[1][0x51] = '3';
	charcode[1][0x52] = '0';
	charcode[1][0x53] = '.';
	charcode[1][0xC7] = KEY_HOME;
	charcode[1][0x9C] = '\n';
	charcode[1][0xB5] = '/';
	charcode[1][0xC8] = KEY_UP;
	charcode[1][0xC9] = KEY_PGUP;
	charcode[1][0xCB] = KEY_LF;
	charcode[1][0xCD] = KEY_RT;
	charcode[1][0xCF] = KEY_END;
	charcode[1][0xD0] = KEY_DN;
	charcode[1][0xD1] = KEY_PGDN;
	charcode[1][0xD2] = KEY_INS;
	charcode[1][0xD3] = KEY_DEL;

	charcode[3][0x10] = C('Q');
	charcode[3][0x11] = C('W');
	charcode[3][0x12] = C('E');
	charcode[3][0x13] = C('R');
	charcode[3][0x14] = C('T');
	charcode[3][0x15] = C('Y');
	charcode[3][0x16] = C('U');
	charcode[3][0x17] = C('I');
	charcode[3][0x18] = C('O');
	charcode[3][0x19] = C('P');
	charcode[3][0x1c] = '\r';
	charcode[3][0x1e] = C('A');
	charcode[3][0x1f] = C('S');
	charcode[3][0x20] = C('D');
	charcode[3][0x21] = C('F');
	charcode[3][0x22] = C('G');
	charcode[3][0x23] = C('H');
	charcode[3][0x24] = C('J');
	charcode[3][0x25] = C('K');
	charcode[3][0x26] = C('L');
	charcode[3][0x2b] = C('\\');
	charcode[3][0x2c] = C('Z');
	charcode[3][0x2d] = C('X');
	charcode[3][0x2e] = C('C');
	charcode[3][0x2f] = C('V');
	charcode[3][0x30] = C('B');
	charcode[3][0x31] = C('N');
	charcode[3][0x32] = C('M');
	charcode[3][0x35] = C('/');
	charcode[3][0x97] = KEY_HOME;
	charcode[3][0xB5] = C('/');
	charcode[3][0xC8] = KEY_UP;
	charcode[3][0xC9] = KEY_PGUP;
	charcode[3][0xCB] = KEY_LF;
	charcode[3][0xCD] = KEY_RT;
	charcode[3][0xCF] = KEY_END;
	charcode[3][0xD0] = KEY_DN;
	charcode[3][0xD1] = KEY_PGDN;
	charcode[3][0xD2] = KEY_INS;
	charcode[3][0xD3] = KEY_DEL;

	// charcode[2] = charcode[3];

	memcpy(charcode[2], charcode[3], 256);
}

void keyboard_handler(registers_t* regs) {
	if(regs){ };
	clean_buffer();
	irq_eoi();
}

void init_keyboard() {
  printf("* Initing keyboard\n");

  clean_buffer();

  set_irq_handler(IRQ_KEYBOARD, irq_keyboard, GD_KT, IDT_PRESENT | IDT_32BIT_INTERRUPT_GATE);

  irq_set_mask(IRQ_KEYBOARD);

	init_keyboard_layout();

  printf("* Keyboard handler: 0x%x%x\n", idt[IRQ_OFFSET + IRQ_KEYBOARD].base_hi, idt[IRQ_OFFSET + IRQ_KEYBOARD].base_lo);
}
