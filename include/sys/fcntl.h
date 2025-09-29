#ifndef MENIOS_INCLUDE_SYS_FCNTL_H
#define MENIOS_INCLUDE_SYS_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define F_DUPFD   0
#define F_GETFD   1
#define F_SETFD   2

#define FD_CLOEXEC (1u)

#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_ACCMODE  0x0003
#define O_CREAT    0x0100
#define O_TRUNC    0x0200
#define O_APPEND   0x0400
#define O_EXCL     0x0800
#define O_DIRECTORY 0x1000
#define O_CLOEXEC  0x2000

#ifdef __cplusplus
}
#endif

#endif
