#ifndef MENIOS_TERMIOS_H
#define MENIOS_TERMIOS_H

#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int tcflag_t;
typedef unsigned int speed_t;
typedef unsigned char cc_t;

#define NCCS 32

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_cc[NCCS];
  speed_t c_ispeed;
  speed_t c_ospeed;
};

/* c_cc character offsets */
#define VINTR  0
#define VQUIT  1
#define VERASE 2
#define VKILL  3
#define VEOF   4
#define VTIME  5
#define VMIN   6
#define VSWTC  7
#define VSTART 8
#define VSTOP  9
#define VSUSP  10
#define VEOL   11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT  15
#define VEOL2   16

/* Input flags */
#define IGNBRK  0x00000001u
#define BRKINT  0x00000002u
#define IGNPAR  0x00000004u
#define PARMRK  0x00000008u
#define INPCK   0x00000010u
#define ISTRIP  0x00000020u
#define INLCR   0x00000040u
#define IGNCR   0x00000080u
#define ICRNL   0x00000100u
#define IXON    0x00000200u
#define IXOFF   0x00000400u

/* Output flags */
#define OPOST   0x00000001u
#define ONLCR   0x00000002u

/* Control flags */
#define CSIZE   0x00000030u
#define CS8     0x00000030u
#define CREAD   0x00000080u
#define PARENB  0x00000100u
#define PARODD  0x00000200u
#define CSTOPB  0x00000400u
#define HUPCL   0x00000800u
#define CLOCAL  0x00001000u

/* Local flags */
#define ISIG    0x00000001u
#define ICANON  0x00000002u
#define ECHO    0x00000008u
#define ECHOE   0x00000010u
#define ECHOK   0x00000020u
#define ECHONL  0x00000040u
#define IEXTEN  0x00000100u

/* Optional actions for tcsetattr */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflow actions */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* speed values */
#define B0      0u
#define B50     50u
#define B75     75u
#define B110    110u
#define B134    134u
#define B150    150u
#define B200    200u
#define B300    300u
#define B600    600u
#define B1200   1200u
#define B1800   1800u
#define B2400   2400u
#define B4800   4800u
#define B9600   9600u
#define B19200  19200u
#define B38400  38400u

/* ioctl requests */
#define TCGETS   _IOR('t', 1, struct termios)
#define TCSETS   _IOW('t', 2, struct termios)
#define TCSETSW  _IOW('t', 3, struct termios)
#define TCSETSF  _IOW('t', 4, struct termios)

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
void cfmakeraw(struct termios* termios_p);
int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);
speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_TERMIOS_H */
