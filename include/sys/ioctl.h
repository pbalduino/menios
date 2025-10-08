#ifndef MENIOS_INCLUDE_SYS_IOCTL_H
#define MENIOS_INCLUDE_SYS_IOCTL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2

#define _IOC_NRSHIFT      0
#define _IOC_TYPESHIFT    (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT    (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT     (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NRMASK   ((1u << _IOC_NRBITS) - 1u)
#define _IOC_TYPEMASK ((1u << _IOC_TYPEBITS) - 1u)
#define _IOC_SIZEMASK ((1u << _IOC_SIZEBITS) - 1u)
#define _IOC_DIRMASK  ((1u << _IOC_DIRBITS) - 1u)

#define _IOC_NONE   0u
#define _IOC_WRITE  1u
#define _IOC_READ   2u

#define _IOC(dir, type, nr, size) \
  ((((unsigned long)(dir))  << _IOC_DIRSHIFT)  | \
   (((unsigned long)(type)) << _IOC_TYPESHIFT) | \
   (((unsigned long)(nr))   << _IOC_NRSHIFT)   | \
   (((unsigned long)(size)) << _IOC_SIZESHIFT))

#define _IOC_DIR(nr)  (((nr) >> _IOC_DIRSHIFT)  & _IOC_DIRMASK)
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)   (((nr) >> _IOC_NRSHIFT)   & _IOC_NRMASK)
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

#define _IO(type, nr)         _IOC(_IOC_NONE,  (type), (nr), 0)
#define _IOR(type, nr, data)  _IOC(_IOC_READ,  (type), (nr), sizeof(data))
#define _IOW(type, nr, data)  _IOC(_IOC_WRITE, (type), (nr), sizeof(data))
#define _IOWR(type, nr, data) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(data))

typedef struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
} winsize_t;

#define TIOCGWINSZ _IOR('t', 104, struct winsize)
#define TIOCSWINSZ _IOW('t', 103, struct winsize)

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif
