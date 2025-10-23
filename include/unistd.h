#ifndef MENIOS_INCLUDE_UNISTD_H
#define MENIOS_INCLUDE_UNISTD_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <sys/types.h>

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
pid_t fork(void);
pid_t getpid(void);
int execve(const char* path, char* const argv[], char* const envp[]);
void _exit(int status) __attribute__((noreturn));
int chdir(const char* path);
char* getcwd(char* buffer, size_t size);
int pause(void);
unsigned int alarm(unsigned int seconds);

int brk(void *addr);

void *sbrk(intptr_t increment);

int open(const char* path, int flags, ...);
int unlink(const char* path);
int rmdir(const char* path);
int access(const char* path, int mode);

#ifdef __cplusplus
}
#endif

#endif
