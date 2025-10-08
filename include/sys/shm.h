#ifndef MENIOS_INCLUDE_SYS_SHM_H
#define MENIOS_INCLUDE_SYS_SHM_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#ifndef IPC_CREAT
#define IPC_CREAT  01000
#endif
#ifndef IPC_EXCL
#define IPC_EXCL   02000
#endif
#ifndef IPC_PRIVATE
#define IPC_PRIVATE ((key_t)0)
#endif

#ifndef SHM_RDONLY
#define SHM_RDONLY 010000
#endif
#ifndef SHM_RND
#define SHM_RND    020000
#endif
#ifndef SHM_REMAP
#define SHM_REMAP  040000
#endif

#ifndef SHM_LOCK
#define SHM_LOCK   11
#endif
#ifndef SHM_UNLOCK
#define SHM_UNLOCK 12
#endif

#define SHM_DEST  01000
#define SHM_LOCKED 02000
#define SHM_LARGEPAGES 04000
#define SHM_NORESERVE 010000

struct shmid_ds {
  struct ipc_perm {
    key_t          key;
    unsigned int   uid;
    unsigned int   gid;
    unsigned short mode;
  } shm_perm;
  size_t   shm_segsz;
  unsigned long shm_atime;
  unsigned long shm_dtime;
  unsigned long shm_ctime;
  unsigned short shm_cpid;
  unsigned short shm_lpid;
  unsigned short shm_nattch;
  unsigned short shm_unused;
  unsigned long  shm_unused2;
  unsigned long  shm_unused3;
};

#ifndef MENIOS_KERNEL
int shmget(key_t key, size_t size, int shmflg);
void* shmat(int shmid, const void* shmaddr, int shmflg);
int shmdt(const void* shmaddr);
int shmctl(int shmid, int cmd, struct shmid_ds* buf);
#endif

#ifdef __cplusplus
}
#endif

#endif
