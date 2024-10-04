#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/mman.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

#include <boot/limine.h>

#include <uacpi/kernel_api.h>

#include <stdio.h>
#include <string.h>

typedef struct uacpi_kmutex {
  bool locked;
} uacpi_kmutex;

typedef struct uacpi_kevent {
  bool signaled;
} uacpi_kevent;

static volatile struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

static void noop() {
  __asm__("nop");
}

uint64_t __popcountdi2(uint64_t x) {
  uint64_t count = 0;
  while(x) {
    count += x & 1;
    x >>= 1;
  }
  return count;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
  for(uacpi_u8 i = 0; i < usec; i++) {
    noop();
  }
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rdsp_address) {
  if(rsdp_request.response == NULL) {
    printf(">>> Error loading device table.\n");
    serial_printf("acpi_init: Error loading device table.\n");
    hcf();
  }

  uintptr_t addr = virtual_to_physical((uintptr_t)rsdp_request.response->address);

  serial_printf("acpi_init: RSDP address: %p\n", addr);

  *out_rdsp_address = addr;

  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_read(
    uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
) {
  switch (byte_width) {
    case 1:
      *out_value = inb(address);
      break;
    case 2:
      *out_value = inw(address);
      break;
    case 4:
      *out_value = inl(address);
      break;
    default:
      serial_printf("uacpi_kernel_raw_io_read: Invalid byte width: %d\n", byte_width);
      return UACPI_STATUS_INVALID_ARGUMENT;
  }

  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 in_value) {
	switch (byte_width) {
		case 1:
			outb(address, in_value);
			break;
		case 2:
			outw(address, in_value);
			break;
		case 4:
			outl(address, in_value);
			break;
		default:
      serial_printf("uacpi_kernel_raw_io_write: Invalid byte width: %d\n", byte_width);
			return UACPI_STATUS_INVALID_ARGUMENT;
	}

	return UACPI_STATUS_OK;
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* msg) {
  serial_printf("%d: %s\n", level, msg);
}

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size) {
  void* obj = kmalloc(count * size);
  memset(obj, 0, count * size);
  return obj;
}

void uacpi_kernel_free(void *mem) {
  kfree(mem);
}

void *uacpi_kernel_alloc(uacpi_size size) {
  return kmalloc(size);
}

uacpi_bool uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16) {
  uacpi_kmutex* mutex = (uacpi_kmutex*)handle;

  serial_printf("uacpi_kernel_acquire_mutex: %p - %d\n", mutex, mutex->locked);

  if(mutex->locked) {
    return false;
  }

  mutex->locked = true;

  return true;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
  uacpi_kmutex* mutex = (uacpi_kmutex*)handle;

  mutex->locked = false;

  serial_printf("uacpi_kernel_release_mutex: %p - %d\n", mutex, mutex->locked);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
  return (void*)physical_to_virtual(addr);
  // return kmmap((void*)addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
  kmunmap(addr, len);
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
  serial_printf("uacpi_kernel_schedule_work not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
  serial_printf("uacpi_kernel_wait_for_work_completion not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
  serial_printf("uacpi_kernel_install_interrupt_handler not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
  serial_printf("uacpi_kernel_uninstall_interrupt_handler not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
  uacpi_kmutex* mutex = (uacpi_kmutex*)handle;
  mutex->locked = true;
  return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags) {
  uacpi_kmutex* mutex = (uacpi_kmutex*)handle;
  mutex->locked = false;
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
  uacpi_kevent* event = (uacpi_kevent*)handle;
  event->signaled = true;
  serial_printf("uacpi_kernel_signal_event: %p - %d\n", event, event->signaled);
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
  uacpi_kevent* event = (uacpi_kevent*)handle;
  event->signaled = false;
  serial_printf("uacpi_kernel_reset_event: %p - %d\n", event, event->signaled);
}

uacpi_handle uacpi_kernel_create_event(void) {
  uacpi_kevent* event = kmalloc(sizeof(uacpi_kevent));
  event->signaled = false;
  serial_printf("uacpi_kernel_create_event: %p - %d\n", event, event->signaled);
  return event;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
  serial_printf("uacpi_kernel_free_event: %p\n", handle);
  kfree(handle);
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
  uacpi_kmutex* event = kmalloc(sizeof(uacpi_kevent));
  event->locked = false;
  serial_printf("uacpi_kernel_create_spinlock: %p - %d\n", event, event->locked);
  return event;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
  kfree(handle);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
  uacpi_kmutex* mutex = kmalloc(sizeof(uacpi_kmutex));
  mutex->locked = false;
  serial_printf("uacpi_kernel_create_mutex: %p\n", mutex);
  return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex) {
  serial_printf("uacpi_kernel_free_mutex: %p\n", mutex);
  kfree(mutex);
}

uacpi_u64 uacpi_kernel_get_ticks(void) {
  serial_printf("uacpi_kernel_get_ticks not implemented\n");
  return 0;
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
  serial_printf("uacpi_kernel_sleep not implemented\n");
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
  serial_printf("uacpi_kernel_wait_for_event not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
  serial_printf("uacpi_kernel_handle_firmware_request not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_read(
    uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
) {
  switch (byte_width) {
    case 1:
      *out_value = *(uacpi_u8*)address;
      break;
    case 2:
      *out_value = *(uacpi_u16*)address;
      break;
    case 4:
      *out_value = *(uacpi_u32*)address;
      break;
    case 8:
      *out_value = *(uacpi_u64*)address;
      break;
    default:
      serial_printf("uacpi_kernel_raw_memory_read: Invalid byte width: %d\n", byte_width);
      return UACPI_STATUS_INVALID_ARGUMENT;
  }

  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(
    uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 in_value
) {
  switch (byte_width) {
    case 1:
      *(uacpi_u8*)address = in_value;
      break;
    case 2:
      *(uacpi_u16*)address = in_value;
      break;
    case 4:
      *(uacpi_u32*)address = in_value;
      break;
    case 8:
      *(uacpi_u64*)address = in_value;
      break;
    default:
      serial_printf("uacpi_kernel_raw_memory_write: Invalid byte width: %d\n", byte_width);
      return UACPI_STATUS_INVALID_ARGUMENT;
  }

  return UACPI_STATUS_OK;
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
  serial_printf("uacpi_kernel_get_thread_id not implemented\n");
  return NULL;
}

uacpi_status uacpi_kernel_pci_read(
    uacpi_pci_address *address, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 *value
) {
  serial_printf("uacpi_kernel_pci_read not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write(
    uacpi_pci_address *address, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 value
) {
  serial_printf("uacpi_kernel_pci_write not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
  serial_printf("uacpi_kernel_io_map not implemented\n");
  return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
  serial_printf("uacpi_kernel_io_unmap not implemented\n");
}

uacpi_status uacpi_kernel_io_read(
    uacpi_handle, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 *value
) {
  serial_printf("uacpi_kernel_io_read not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write(
    uacpi_handle, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 value
) {
  serial_printf("uacpi_kernel_io_write not implemented\n");
  return UACPI_STATUS_OK;
}


uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl) {
  serial_printf("uacpi_kernel_initialize: not implemented. Init level: %d\n", current_init_lvl);

  return UACPI_STATUS_OK;
}

void uacpi_kernel_deinitialize(void) {
  serial_printf("uacpi_kernel_deinitialize: not implemented\n");
}
