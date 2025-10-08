# meniOS Shared Memory Plan

## Scope

This document captures the implementation strategy for the shared
memory roadmap items (#215 – #219). It explains the kernel data
structures, syscall surface, and the userland developer experience, so
each issue can land incrementally without losing sight of the full
feature.

## Current Status

- No `shm*` APIs exist in kernel or userland today; processes can only
  exchange data via pipes or files.
- The virtual memory manager already supports anonymous mappings and
  copy-on-write fork semantics, which we can leverage to back shared
  regions.
- The task backlog is split as follows:
  1. **#215** – Kernel region manager
  2. **#216** – Syscalls & libc wrappers (`shmget`, `shmat`, `shmdt`, `shmctl`)
  3. **#217** – Reference counting & cleanup
  4. **#218** – Regression test suite
  5. **#219** – Documentation & examples (this document)

## Architecture Overview

### Kernel Objects

| Concept            | Responsibility                                                     |
|--------------------|-------------------------------------------------------------------|
| `shm_id`           | 32-bit identifier, allocated from a global table keyed by `key_t`. |
| `shm_region`       | Holds size, backing physical pages, reference counts.              |
| `shm_attachment`   | Per-process mapping metadata (address, flags).                     |
| `shm_table`        | Global hash map protected by `kmutex` + `kcondvar` for waiters.    |

Backed pages live in the physical memory manager. When the last
attachment drops, the backing pages are returned to the PMM (Issue
#217).

### Syscalls (Issue #216)

| Syscall            | Prototype (userland)                                      | Notes                                        |
|--------------------|-----------------------------------------------------------|----------------------------------------------|
| `SYS_SHMGET`       | `int shmget(key_t key, size_t size, int shmflg);`         | Creates or locates a region.                 |
| `SYS_SHMAT`        | `void* shmat(int shmid, const void* addr, int shmflg);`   | Attaches into caller VM space.               |
| `SYS_SHMDT`        | `int shmdt(const void* addr);`                            | Detaches a prior `shmat`.                    |
| `SYS_SHMCTL`       | `int shmctl(int shmid, int cmd, struct shmid_ds* buf);`   | Query/destroy/update region metadata.        |

`SYS_SHMGET` will support System V semantics (`IPC_CREAT`, `IPC_EXCL`) and
the special `IPC_PRIVATE` key for anonymous segments.

### Virtual Memory Integration

- Each attachment reserves a `vm_region` with page-aligned size and
  shared physical frame list.
- `fork()` inherits attachments by bumping per-region reference counts;
  the child still receives the same virtual address (System V
  requirement).
- When a process exits, `proc_vm_cleanup()` walks attachments and
  decrements the region reference count.

## Incremental Delivery Checklist

1. **Region Manager (#215)**
   - Implement `shm_region_create`, `shm_region_get`, `shm_region_put`.
   - Track `shm_nattch` (attachment count) and `shm_atime/mtime/ctime` timestamps.
   - Provide kernel-only helpers for `proc_shm_attach` / `proc_shm_detach`.
2. **Syscalls & libc (#216)**
   - Add syscall numbers in `include/menios/syscall.h` and dispatcher stubs.
   - Implement libc wrappers in `src/libc/unistd.c` or a new `src/libc/sysv_ipc.c`.
   - Export headers (`<sys/shm.h>`) mirroring POSIX definitions.
3. **Cleanup & Signals (#217)**
   - Enforce `IPC_RMID` semantics via `shmctl` so new attaches fail while
     existing processes continue until they detach.
   - Drop the region when `shm_nattch` hits zero after `IPC_RMID`.
4. **Testing (#218)**
   - Host Unity tests covering allocation, double-attachment, and cleanup.
   - QEMU smoke-test: parent + child exchanging a large struct.
5. **Documentation (#219)**
   - This doc.
   - Example programs (below) staged under `docs/examples/` once the
     syscalls land.

## Example Programs (Planned)

> **Note**: The following samples illustrate the intended API once
> Issues #215-#217 are complete. They will not run until the syscalls
> land.

### 1. Parent/Child Message Ring

```c
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  volatile int write_index;
  char payload[4][64];
} ring_buffer_t;

int main(void) {
  int shmid = shmget(IPC_PRIVATE, sizeof(ring_buffer_t), IPC_CREAT | 0600);
  if(shmid < 0) {
    perror("shmget");
    return 1;
  }

  ring_buffer_t* ring = shmat(shmid, NULL, 0);
  if(ring == (void*)-1) {
    perror("shmat");
    return 1;
  }
  memset(ring, 0, sizeof(*ring));

  pid_t child = fork();
  if(child == 0) {
    for(int i = 0; i < 4; ++i) {
      while(ring->write_index <= i) { /* busy wait */ }
      printf("child saw: %s\n", ring->payload[i]);
    }
    _exit(0);
  }

  const char* messages[] = {"hello", "shared", "memory", "world"};
  for(int i = 0; i < 4; ++i) {
    strcpy(ring->payload[i], messages[i]);
    __atomic_store_n(&ring->write_index, i + 1, __ATOMIC_SEQ_CST);
  }

  waitpid(child, NULL, 0);
  shmdt(ring);
  shmctl(shmid, IPC_RMID, NULL);
  return 0;
}
```

### 2. Logger + Worker Processes

```c
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
  volatile size_t count;
  unsigned long values[1024];
} metrics_t;

int main(void) {
  key_t key = ftok("/bin/mosh", 0x42);
  int shmid = shmget(key, sizeof(metrics_t), IPC_CREAT | 0660);
  metrics_t* metrics = shmat(shmid, NULL, 0);
  metrics->count = 0;

  for(int worker = 0; worker < 4; ++worker) {
    if(fork() == 0) {
      for(int i = 0; i < 100; ++i) {
        size_t idx = __atomic_fetch_add(&metrics->count, 1, __ATOMIC_SEQ_CST);
        metrics->values[idx % 1024] = (unsigned long)getpid() << 32 | i;
      }
      _exit(0);
    }
  }

  while(wait(NULL) > 0) { }
  printf("total metrics: %zu\n", metrics->count);

  shmdt(metrics);
  shmctl(shmid, IPC_RMID, NULL);
  return 0;
}
```

## Testing Strategy (Issue #218)

| Layer      | Tests                                                                 |
|------------|-----------------------------------------------------------------------|
| Kernel     | Unit tests for `shm_region` lifecycle; integration tests via host VM. |
| Userland   | Unity suites linking against libc wrappers (see `test/test_syscall_shm.c`). |
| QEMU       | Smoke tests run from CI to ensure segments survive `fork/exec`.       |

## Documentation Rollout

- Update the meniOS manual once syscalls ship (exported via `include/sys/shm.h`).
- Add a quick-start guide under `docs/examples/` with the sample programs
  above once they compile against the real API.
- Advertise shared memory in the README roadmap under the IPC milestones when
  all subtasks close.
