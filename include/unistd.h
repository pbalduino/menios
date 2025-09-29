#ifndef MENIOS_INCLUDE_UNISTD_H
#define MENIOS_INCLUDE_UNISTD_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef long ssize_t;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

ssize_t read(int fd, void* buffer, size_t length);
ssize_t write(int fd, const void* buffer, size_t length);
int close(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int pipefd[2]);
off_t lseek(int fd, off_t offset, int whence);
int kill(pid_t pid, int sig);

int brk(void *addr);

void *sbrk(intptr_t increment);

#ifdef __cplusplus
}
#endif

#endif
