#ifndef MENIOS_INCLUDE_UNISTD_H
#define MENIOS_INCLUDE_UNISTD_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef long ssize_t;

ssize_t read(int fd, void* buffer, size_t length);
ssize_t write(int fd, const void* buffer, size_t length);
int close(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int pipefd[2]);

int brk(void *addr);

void *sbrk(intptr_t increment);

#ifdef __cplusplus
}
#endif

#endif
