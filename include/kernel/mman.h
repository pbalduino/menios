#ifndef MENIOS_INCLUDE_KERNEL_MMAN_H
#define MENIOS_INCLUDE_KERNEL_MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> // size_t
#include <types.h>

#define	PROT_NONE  0x00 // no access
#define	PROT_READ  0x01 // pages can be read
#define	PROT_WRITE 0x02 // pages can be written
#define	PROT_EXEC  0x04 // pages can be executed

#define MAP_SHARED    0x0001
#define MAP_PRIVATE   0x0002    // changes are private
#define	MAP_ANON      0x1000    // allocated from memory
#define	MAP_ANONYMOUS MAP_ANON  // alternate POSIX spelling

#define MAP_FAILED ((void *) -1) // mapping failed

void* kmmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   kmunmap(void *addr, size_t len);

#ifdef __cplusplus
}
#endif
#endif //MENIOS_INCLUDE_KERNEL_MMAN_H