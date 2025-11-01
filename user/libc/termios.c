#ifndef MENIOS_KERNEL
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int termios_request_from_action(int action) {
  switch(action) {
    case TCSANOW:
      return TCSETS;
    case TCSADRAIN:
      return TCSETSW;
    case TCSAFLUSH:
      return TCSETSF;
    default:
      errno = EINVAL;
      return -1;
  }
}

int tcgetattr(int fd, struct termios* termios_p) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }
  return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }
  int request = termios_request_from_action(optional_actions);
  if(request < 0) {
    return -1;
  }
  return ioctl(fd, (unsigned long)request, (void*)termios_p);
}

void cfmakeraw(struct termios* termios_p) {
  if(termios_p == NULL) {
    return;
  }
  termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                          INLCR  | IGNCR | ICRNL | IXON);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_cflag &= ~(CSIZE | PARENB);
  termios_p->c_cflag |= CS8 | CREAD;
  termios_p->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL |
                          ICANON | ISIG | IEXTEN);
  termios_p->c_cc[VMIN] = 1;
  termios_p->c_cc[VTIME] = 0;
}

int cfsetispeed(struct termios* termios_p, speed_t speed) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }
  termios_p->c_ispeed = speed;
  return 0;
}

int cfsetospeed(struct termios* termios_p, speed_t speed) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }
  termios_p->c_ospeed = speed;
  return 0;
}

speed_t cfgetispeed(const struct termios* termios_p) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return (speed_t)0;
  }
  return termios_p->c_ispeed;
}

speed_t cfgetospeed(const struct termios* termios_p) {
  if(termios_p == NULL) {
    errno = EINVAL;
    return (speed_t)0;
  }
  return termios_p->c_ospeed;
}
#endif
