#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
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

pid_t getpid(void) {
  long rc = __menios_syscall0(SYS_GETPID);
  if(rc < 0) {
    errno = (int)(-rc);
    return (pid_t)-1;
  }

  errno = 0;
  return (pid_t)rc;
}

pid_t waitpid(pid_t pid, int* status, int options) {
  long rc = __menios_syscall3(SYS_WAITPID, (long)pid, (long)status, (long)options);
  if(rc < 0) {
    errno = (int)(-rc);
    return (pid_t)-1;
  }

  errno = 0;
  return (pid_t)rc;
}

pid_t wait(int* status) {
  return waitpid((pid_t)-1, status, 0);
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

int execv(const char* path, char* const argv[]) {
  extern char** environ;
  return execve(path, argv, environ);
}

int execvp(const char* file, char* const argv[]) {
  extern char** environ;
  if(file == NULL || *file == '\0') {
    errno = ENOENT;
    return -1;
  }

  if(strchr(file, '/') != NULL) {
    return execve(file, argv, environ);
  }

  const char* path = getenv("PATH");
  if(path == NULL || *path == '\0') {
    path = "/bin:/usr/bin";
  }

  size_t file_len = strlen(file);
  int saved_errno = ENOENT;
  int saw_eacces = 0;

  const char* cursor = path;
  while(*cursor != '\0') {
    const char* colon = strchr(cursor, ':');
    size_t dir_len = (colon != NULL) ? (size_t)(colon - cursor) : strlen(cursor);

    const char* dir_ptr;
    size_t dir_size;
    if(dir_len == 0) {
      dir_ptr = ".";
      dir_size = 1;
    } else {
      dir_ptr = cursor;
      dir_size = dir_len;
    }

    int needs_slash = (dir_size > 0 && dir_ptr[dir_size - 1] == '/') ? 0 : 1;
    size_t total = dir_size + needs_slash + file_len + 1;

    char* candidate = (char*)malloc(total);
    if(candidate == NULL) {
      errno = ENOMEM;
      return -1;
    }

    memcpy(candidate, dir_ptr, dir_size);
    size_t pos = dir_size;
    if(needs_slash) {
      candidate[pos++] = '/';
    }
    memcpy(candidate + pos, file, file_len);
    candidate[pos + file_len] = '\0';

    int rc = execve(candidate, argv, environ);
    if(rc >= 0) {
      free(candidate);
      return rc;
    }

    int current_errno = errno;
    if(current_errno == EACCES) {
      saw_eacces = 1;
    } else if(current_errno != ENOENT) {
      saved_errno = current_errno;
    } else if(saved_errno == ENOENT) {
      saved_errno = ENOENT;
    }

    free(candidate);

    if(colon == NULL) {
      break;
    }
    cursor = colon + 1;
  }

  errno = saw_eacces ? EACCES : saved_errno;
  return -1;
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

int unlink(const char* path) {
  long rc = __menios_syscall1(SYS_UNLINK, (long)path);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

int rmdir(const char* path) {
  long rc = __menios_syscall1(SYS_RMDIR, (long)path);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

int access(const char* path, int mode) {
  (void)path;
  (void)mode;
  errno = ENOSYS;
  return -1;
}

int isatty(int fd) {
  (void)fd;
  errno = ENOTTY;
  return 0;
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
