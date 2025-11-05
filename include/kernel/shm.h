#ifndef MENIOS_INCLUDE_KERNEL_SHM_H
#define MENIOS_INCLUDE_KERNEL_SHM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <types.h>

#include <kernel/pmm.h>

typedef int32_t shm_key_t;

typedef struct shm_region shm_region_t;

typedef enum {
  SHM_REGION_CREATE_OK = 0,
  SHM_REGION_CREATE_INVALID_ARGUMENT = -1,
  SHM_REGION_CREATE_NO_MEMORY = -2
} shm_region_create_status_t;

#define SHMLBA 4096

#ifndef IPC_CREAT
#define IPC_CREAT  01000
#endif
#ifndef IPC_EXCL
#define IPC_EXCL   02000
#endif
#ifndef IPC_PRIVATE
#define IPC_PRIVATE ((shm_key_t)0)
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

#ifndef IPC_RMID
#define IPC_RMID   0
#endif
#ifndef IPC_SET
#define IPC_SET    1
#endif
#ifndef IPC_STAT
#define IPC_STAT   2
#endif

typedef phys_frame_t (*shm_page_alloc_fn)(size_t page_count);
typedef void (*shm_page_free_fn)(phys_frame_t base, size_t page_count);

void shm_manager_initialize(void);

void shm_set_allocator(shm_page_alloc_fn alloc_fn,
                       shm_page_free_fn free_fn);

shm_region_t* shm_region_create(shm_key_t key,
                                size_t size,
                                uint16_t mode,
                                int* id_out,
                                shm_region_create_status_t* status_out);

shm_region_t* shm_region_get_by_id(int id);
shm_region_t* shm_region_get_by_key(shm_key_t key);

void shm_region_ref(shm_region_t* region);
void shm_region_unref(shm_region_t* region);

int shm_region_id(const shm_region_t* region);
shm_key_t shm_region_key(const shm_region_t* region);
size_t shm_region_size(const shm_region_t* region);
size_t shm_region_page_count(const shm_region_t* region);
phys_addr_t shm_region_page(const shm_region_t* region, size_t index);
uint16_t shm_region_mode(const shm_region_t* region);

void shm_region_set_marked_for_removal(shm_region_t* region, bool value);
bool shm_region_marked_for_removal(shm_region_t* region);

void shm_region_increment_attachments(shm_region_t* region);
void shm_region_decrement_attachments(shm_region_t* region);
size_t shm_region_attachment_count(shm_region_t* region);

#ifdef __cplusplus
}
#endif

#endif
