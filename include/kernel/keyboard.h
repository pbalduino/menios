#ifndef MENIOS_INCLUDE_KERNEL_KEYBOARD_H
#define MENIOS_INCLUDE_KERNEL_KEYBOARD_H

#include <types.h>

#define	KBSTATP		0x64	/* kbd controller status port(I) */
#define	 KBS_DIB	0x01	/* kbd data in buffer */
#define	 KBS_IBF	0x02	/* kbd input buffer low */
#define	 KBS_WARM	0x04	/* kbd input buffer low */
#define	 KBS_OCMD	0x08	/* kbd output buffer has command */
#define	 KBS_NOSEC	0x10	/* kbd security lock not engaged */
#define	 KBS_FROM_MOUSE	0x20	/* kbd transmission error or from mouse */
#define	 KBS_RERR	0x40	/* kbd receive error */
#define	 KBS_PERR	0x80	/* kbd parity error */

void init_keyboard();

#endif
