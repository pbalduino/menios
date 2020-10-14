#ifndef MENIOS_INCLUDE_KERNEL_KEYBOARD_H
#define MENIOS_INCLUDE_KERNEL_KEYBOARD_H

#define	KBCMDP		0x64	/* kbd controller port(O) */
#define	KBSTATP		0x64	/* kbd controller status port(I) */

#define	KBS_DIB	0x01	/* kbd data in buffer */
#define	KBDATAP		0x60	/* kbd data port(I) */
#define	KBOUTP		0x60	/* kbd data port(O) */

// Special keycodes
#define KEY_HOME	0xE0
#define KEY_END		0xE1
#define KEY_UP		0xE2
#define KEY_DN		0xE3
#define KEY_LF		0xE4
#define KEY_RT		0xE5
#define KEY_PGUP	0xE6
#define KEY_PGDN	0xE7
#define KEY_INS		0xE8
#define KEY_DEL		0xE9

void init_keyboard();

#endif
