#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

ssize_t read(int fd, void* buffer, size_t length) {
  long rc = __menios_syscall3(SYS_READ, (long)fd, (long)buffer, (long)length);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (ssize_t)rc;
}

ssize_t write(int fd, const void* buffer, size_t length) {
  long rc = __menios_syscall3(SYS_WRITE, (long)fd, (long)buffer, (long)length);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (ssize_t)rc;
}

int close(int fd) {
  long rc = __menios_syscall1(SYS_CLOSE, (long)fd);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

int dup(int fd) {
  long rc = __menios_syscall1(SYS_DUP, (long)fd);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int dup2(int oldfd, int newfd) {
  long rc = __menios_syscall2(SYS_DUP2, (long)oldfd, (long)newfd);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int pipe(int pipefd[2]) {
  long rc = __menios_syscall1(SYS_PIPE, (long)pipefd);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
  long rc = __menios_syscall3(SYS_LSEEK,
                              (long)fd,
                              (long)offset,
                              (long)whence);

  if(rc < 0) {
    errno = (int)(-rc);
    return (off_t)-1;
  }

  errno = 0;
  return (off_t)rc;
}

int ioctl(int fd, unsigned long request, ...) {
  va_list ap;
  va_start(ap, request);
  void* argp = va_arg(ap, void*);
  va_end(ap);

  long rc = __menios_syscall3(SYS_IOCTL,
                              (long)fd,
                              (long)request,
                              (long)argp);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

pid_t fork(void) {
  long rc = __menios_syscall0(SYS_FORK);
  if(rc < 0) {
    errno = (int)(-rc);
    return (pid_t)-1;
  }

  errno = 0;
  return (pid_t)rc;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
  long rc = __menios_syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return (int)rc;
}

int chdir(const char* path) {
  long rc = __menios_syscall1(SYS_CHDIR, (long)path);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

char* getcwd(char* buffer, size_t size) {
  long rc = __menios_syscall2(SYS_GETCWD, (long)buffer, (long)size);
  if(rc < 0) {
    errno = (int)(-rc);
    return NULL;
  }

  errno = 0;
  return (char*)rc;
}

int brk(void* addr) {
  (void)addr;
  errno = ENOSYS;
  return -1;
}

void* sbrk(intptr_t increment) {
  (void)increment;
  errno = ENOSYS;
  return (void*)-1;
}

#endif
