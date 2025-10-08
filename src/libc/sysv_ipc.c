#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <sys/errno.h>
#include <sys/shm.h>

int shmget(key_t key, size_t size, int shmflg) {
  long rc = __menios_syscall3(SYS_SHMGET, (long)key, (long)size, (long)shmflg);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  errno = 0;
  return (int)rc;
}

void* shmat(int shmid, const void* shmaddr, int shmflg) {
  long rc = __menios_syscall3(SYS_SHMAT, (long)shmid, (long)shmaddr, (long)shmflg);
  if(rc < 0) {
    errno = (int)(-rc);
    return (void*)-1;
  }
  errno = 0;
  return (void*)rc;
}

int shmdt(const void* shmaddr) {
  long rc = __menios_syscall1(SYS_SHMDT, (long)shmaddr);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  errno = 0;
  return 0;
}

int shmctl(int shmid, int cmd, struct shmid_ds* buf) {
  long rc = __menios_syscall3(SYS_SHMCTL, (long)shmid, (long)cmd, (long)buf);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  errno = 0;
  return (int)rc;
}
#endif
