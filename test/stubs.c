#include <kernel/mutex.h>
#include <stdlib.h>
#include <string.h>

void cli() {}

void sti() {}

void hcf() {
  exit(1);
}

int kmutex_lock(kmutex_t* mutex) { }

int kmutex_unlock(kmutex_t* mutex) { }

void memzero(void* s, uint64_t n) {
	memset(s, 0, n);
}
