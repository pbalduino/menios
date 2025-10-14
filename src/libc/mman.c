#ifndef MENIOS_KERNEL
#include <menios/syscall.h>
#include <menios/syscall_user.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t log_append_str(char* buffer, size_t pos, size_t capacity, const char* text) {
  while(text != NULL && *text != '\0' && pos < capacity) {
    buffer[pos++] = *text++;
  }
  return pos;
}

static size_t log_append_hex(char* buffer, size_t pos, size_t capacity, uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  char tmp[16];
  size_t idx = 0u;

  if(value == 0) {
    tmp[idx++] = '0';
  } else {
    while(value != 0 && idx < sizeof(tmp)) {
      tmp[idx++] = digits[value & 0xFu];
      value >>= 4u;
    }
  }

  while(idx > 0u && pos < capacity) {
    buffer[pos++] = tmp[--idx];
  }

  return pos;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  long rc = __menios_syscall6(SYS_MMAP,
                              (long)addr,
                              (long)length,
                              (long)prot,
                              (long)flags,
                              (long)fd,
                              (long)offset);
  long rc_trace = __menios_syscall_last_result;

  {
    char rawbuf[64];
    size_t rawpos = 0u;
    rawpos = log_append_str(rawbuf, rawpos, sizeof(rawbuf), "mmap raw rc=0x");
    rawpos = log_append_hex(rawbuf, rawpos, sizeof(rawbuf), (uint64_t)rc);
    rawpos = log_append_str(rawbuf, rawpos, sizeof(rawbuf), " trace=0x");
    rawpos = log_append_hex(rawbuf, rawpos, sizeof(rawbuf), (uint64_t)rc_trace);
    if(rawpos < sizeof(rawbuf)) {
      rawbuf[rawpos++] = '\n';
    }
    (void)write(2, rawbuf, rawpos);
  }

  if(rc < 0) {
    errno = (int)(-rc);
    return MAP_FAILED;
  }

  errno = 0;

  uintptr_t result = (uintptr_t)rc;
  uintptr_t result_hi = result >> 32;

  {
    char buf[128];
    size_t pos = 0u;
    pos = log_append_str(buf, pos, sizeof(buf), "mmap: addr=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)(uintptr_t)addr);
    pos = log_append_str(buf, pos, sizeof(buf), " len=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)length);
    pos = log_append_str(buf, pos, sizeof(buf), " ret=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)result);
    pos = log_append_str(buf, pos, sizeof(buf), " hi=0x");
    pos = log_append_hex(buf, pos, sizeof(buf), (uint64_t)result_hi);
    if(pos < sizeof(buf)) {
      buf[pos++] = '\n';
    }
    (void)write(2, buf, pos);
  }

  return (void*)result;
}

int munmap(void* addr, size_t length) {
  long rc = __menios_syscall2(SYS_MUNMAP, (long)addr, (long)length);

  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}
#endif
