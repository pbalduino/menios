#ifndef MENIOS_INCLUDE_SYS_WAIT_H
#define MENIOS_INCLUDE_SYS_WAIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WNOHANG     0x01
#define WUNTRACED   0x02
#define WCONTINUED  0x04

#define WIFEXITED(status)     (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)   (((status) >> 8) & 0xff)
#define WIFSIGNALED(status)   ((((status) & 0x7f) != 0) && (((status) & 0x7f) != 0x7f) && (((status) & 0x80) == 0))
#define WTERMSIG(status)      ((status) & 0x7f)
#define WCOREDUMP(status)     (((status) & 0x80) != 0)
#define WIFSTOPPED(status)    (((status) & 0x7f) == 0x7f)
#define WSTOPSIG(status)      (((status) >> 8) & 0xff)
#define WIFCONTINUED(status)  ((status) == 0xffff)

#ifdef __cplusplus
}
#endif

#endif
