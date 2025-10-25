#ifndef MENIOS_INCLUDE_UNISTD_H
#define MENIOS_INCLUDE_UNISTD_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <sys/types.h>

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif

#ifndef _PC_PATH_MAX
#define _PC_PATH_MAX 1
#endif

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
int execv(const char* path, char* const argv[]);
int execvp(const char* file, char* const argv[]);
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
int isatty(int fd);
long pathconf(const char* path, int name);

#ifdef __cplusplus
}
#endif

#endif
