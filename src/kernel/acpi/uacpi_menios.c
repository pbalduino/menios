#include <kernel/acpi.h>
#include <kernel/heap.h>
#include <kernel/kernel.h>
#include <kernel/mman.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>
#include <kernel/tsc.h>
#include <kernel/pmm.h>
#include <kernel/proc.h>
#include <kernel/serial.h>
#include <kernel/pci.h>
#include <kernel/irq.h>

#include <boot/limine.h>

#include <uacpi/kernel_api.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct uacpi_kevent {
  ksem_t sem;
} uacpi_kevent;

typedef struct uacpi_irq_binding {
  struct irq_handle* irq_handle;
  uacpi_interrupt_handler handler;
  uacpi_handle ctx;
} uacpi_irq_binding;

static bool uacpi_irq_dispatch(void* opaque) {
  if(opaque == NULL) {
    return false;
  }

  uacpi_irq_binding* binding = (uacpi_irq_binding*)opaque;
  if(binding->handler == NULL) {
    return false;
  }

  uacpi_interrupt_ret result = binding->handler(binding->ctx);
  return result != UACPI_INTERRUPT_NOT_HANDLED;
}

static volatile struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

uint64_t __popcountdi2(uint64_t x) {
  uint64_t count = 0;
  while(x) {
    count += x & 1;
    x >>= 1;
  }
  return count;
}

typedef struct {
  bool initialized;
  uacpi_init_level current_level;
} uacpi_kernel_init_state_t;

static uacpi_kernel_init_state_t uacpi_kernel_init_state = {
  .initialized = false,
  .current_level = UACPI_INIT_LEVEL_EARLY
};

static const char* uacpi_kernel_init_level_name(uacpi_init_level level) {
  switch(level) {
    case UACPI_INIT_LEVEL_EARLY: return "early";
    case UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED: return "subsystem initialized";
    case UACPI_INIT_LEVEL_NAMESPACE_LOADED: return "namespace loaded";
    case UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED: return "namespace initialized";
    default: return "unknown";
  }
}

static uacpi_status uacpi_kernel_run_init_stage(uacpi_init_level level) {
  (void)level;
  return UACPI_STATUS_OK;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
  for(uacpi_u8 i = 0; i < usec; i++) {
    noop();
  }
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rdsp_address) {
  if(rsdp_request.response == NULL) {
    printf(">>> Error loading device table.\n");
    serial_printf("acpi_initialize: Error loading device table.\n");
    halt();
  }

  uintptr_t addr = virtual_to_physical((uintptr_t)rsdp_request.response->address);

  serial_printf("acpi_initialize: RSDP address: %p\n", addr);

  *out_rdsp_address = addr;

  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_read(
    uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
) {
  switch(byte_width) {
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
	switch(byte_width) {
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
  kmutex_t* mutex = (kmutex_t*)handle;
  kmutex_lock(mutex);

  return true;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
  kmutex_t* mutex = (kmutex_t*)handle;
  kmutex_unlock(mutex);
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
    uacpi_u32 irq,
    uacpi_interrupt_handler handler,
    uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
  if(handler == NULL || out_irq_handle == NULL) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  uacpi_irq_binding* binding = kmalloc(sizeof(*binding));
  if(binding == NULL) {
    return UACPI_STATUS_OUT_OF_MEMORY;
  }

  binding->handler = handler;
  binding->ctx = ctx;
  binding->irq_handle = NULL;

  int rc = irq_register(irq, uacpi_irq_dispatch, binding, &binding->irq_handle);
  if(rc != 0) {
    kfree(binding);
    if(rc == -ENOMEM) {
      return UACPI_STATUS_OUT_OF_MEMORY;
    }
    return UACPI_STATUS_ERROR;
  }

  *out_irq_handle = (uacpi_handle)binding;
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler,
    uacpi_handle irq_handle
) {
  if(irq_handle == UACPI_NULL) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  uacpi_irq_binding* binding = (uacpi_irq_binding*)irq_handle;
  if(handler != NULL && handler != binding->handler) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  if(binding->irq_handle == NULL) {
    kfree(binding);
    return UACPI_STATUS_ERROR;
  }

  int rc = irq_unregister(binding->irq_handle);
  binding->irq_handle = NULL;

  kfree(binding);

  if(rc != 0) {
    return UACPI_STATUS_ERROR;
  }

  return UACPI_STATUS_OK;
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
  spinlock_t* lock = (spinlock_t*)handle;
  return (uacpi_cpu_flags)spinlock_lock_irqsave(lock);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
  spinlock_t* lock = (spinlock_t*)handle;
  spinlock_unlock_irqrestore(lock, (uint64_t)flags);
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
  if(handle == NULL) {
    return;
  }

  uacpi_kevent* event = (uacpi_kevent*)handle;
  ksem_post(&event->sem);
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
  if(handle == NULL) {
    return;
  }

  uacpi_kevent* event = (uacpi_kevent*)handle;
  while(ksem_trywait(&event->sem)) {
  }
}

uacpi_handle uacpi_kernel_create_event(void) {
  uacpi_kevent* event = kmalloc(sizeof(*event));
  if(event == NULL) {
    return NULL;
  }

  ksem_initialize(&event->sem, 0);
  return event;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
  if(handle == NULL) {
    return;
  }

  uacpi_kevent* event = (uacpi_kevent*)handle;
  ksem_destroy(&event->sem);
  kfree(event);
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
  spinlock_t* lock = kmalloc(sizeof(spinlock_t));
  spinlock_init(lock);
  serial_printf("uacpi_kernel_create_spinlock: %p\n", lock);
  return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
  serial_printf("uacpi_kernel_free_spinlock: %p\n", handle);
  kfree(handle);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
  kmutex_t* mutex = kmalloc(sizeof(kmutex_t));
  memset(mutex, 0, sizeof(kmutex_t));
  kmutex_init(mutex);
  return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex) {
  kfree(mutex);
}

uacpi_u64 uacpi_kernel_get_ticks(void) {
  uint64_t ns = ns_from_boot();
  return ns / 100ull;
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
  if(msec == 0) {
    return;
  }

  ksleep((uint64_t)msec);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
  if(handle == NULL) {
    return UACPI_FALSE;
  }

  uacpi_kevent* event = (uacpi_kevent*)handle;

  if(ksem_trywait(&event->sem)) {
    return UACPI_TRUE;
  }

  if(timeout == 0) {
    return UACPI_FALSE;
  }

  if(timeout == 0xFFFF) {
    ksem_wait(&event->sem);
    return UACPI_TRUE;
  }

  uacpi_u16 remaining = timeout;
  while(remaining > 0) {
    if(ksem_trywait(&event->sem)) {
      return UACPI_TRUE;
    }

    ksleep(1);
    if(remaining > 1) {
      remaining -= 1;
    } else {
      remaining = 0;
    }
  }

  return UACPI_FALSE;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
  serial_printf("uacpi_kernel_handle_firmware_request not implemented\n");
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_read(
    uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
) {
  switch(byte_width) {
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
  switch(byte_width) {
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
  if(current == NULL) {
    return NULL;
  }

  return (uacpi_thread_id)current;
}

static uacpi_status uacpi_validate_pci_access(uacpi_pci_address* address,
                                             uacpi_size offset,
                                             uacpi_u8 byte_width,
                                             uacpi_u64* value) {
  if(address == NULL || value == NULL) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  if(byte_width == 0 || byte_width > 4) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  if(offset + byte_width > 4096) {
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read(
    uacpi_pci_address *address,
    uacpi_size offset,
    uacpi_u8 byte_width,
    uacpi_u64 *value
) {
  uacpi_status status = uacpi_validate_pci_access(address, offset, byte_width, value);
  if(status != UACPI_STATUS_OK) {
    return status;
  }

  uint16_t segment = (uint16_t)address->segment;
  uint8_t bus = (uint8_t)address->bus;
  uint8_t device = (uint8_t)address->device;
  uint8_t function = (uint8_t)address->function;

  uacpi_u64 result = 0;

  for(uacpi_size i = 0; i < byte_width; ++i) {
    uacpi_size byte_offset = offset + i;
    uint8_t aligned = (uint8_t)(byte_offset & ~0x3u);
    uint32_t raw = pci_config_read_segment(segment, bus, device, function, aligned);
    uint32_t shift = (uint32_t)(byte_offset & 0x3u) * 8u;
    uacpi_u64 byte_value = (raw >> shift) & 0xFFu;
    result |= byte_value << (i * 8u);
  }

  *value = result;
  return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write(
    uacpi_pci_address *address,
    uacpi_size offset,
    uacpi_u8 byte_width,
    uacpi_u64 value
) {
  uacpi_u64 dummy;
  uacpi_status status = uacpi_validate_pci_access(address, offset, byte_width, &dummy);
  if(status != UACPI_STATUS_OK) {
    return status;
  }

  uint16_t segment = (uint16_t)address->segment;
  uint8_t bus = (uint8_t)address->bus;
  uint8_t device = (uint8_t)address->device;
  uint8_t function = (uint8_t)address->function;

  uacpi_size start = offset;
  uacpi_size end = offset + byte_width;

  for(uacpi_size aligned = start & ~0x3u; aligned < end; aligned += 4) {
    uint32_t raw = pci_config_read_segment(segment,
                                           bus,
                                           device,
                                           function,
                                           (uint8_t)aligned);

    for(uacpi_size i = 0; i < 4; ++i) {
      uacpi_size byte_index = aligned + i;
      if(byte_index < start || byte_index >= end) {
        continue;
      }

      uacpi_size value_index = byte_index - start;
      uint32_t byte_value = (uint32_t)((value >> (value_index * 8u)) & 0xFFu);
      raw &= ~(0xFFu << (i * 8u));
      raw |= byte_value << (i * 8u);
    }

    pci_config_write_segment(segment,
                             bus,
                             device,
                             function,
                             (uint8_t)aligned,
                             raw);
  }

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
  serial_printf("uacpi_kernel_io_write not implemented - offset: %lx - width: %lx - value: %lx\n", offset, byte_width, value);
  return UACPI_STATUS_OK;
}


uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl) {
  if(current_init_lvl > UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED) {
    serial_printf("uacpi_kernel_initialize: invalid init level %d\n", current_init_lvl);
    return UACPI_STATUS_INVALID_ARGUMENT;
  }

  if(!uacpi_kernel_init_state.initialized) {
    if(current_init_lvl != UACPI_INIT_LEVEL_EARLY) {
      serial_printf("uacpi_kernel_initialize: first call must be EARLY, got %s\n",
                    uacpi_kernel_init_level_name(current_init_lvl));
      return UACPI_STATUS_INIT_LEVEL_MISMATCH;
    }

    uacpi_kernel_init_state.initialized = true;
    uacpi_kernel_init_state.current_level = current_init_lvl;

    serial_printf("uacpi_kernel_initialize: level -> %s\n",
                  uacpi_kernel_init_level_name(current_init_lvl));
    return uacpi_kernel_run_init_stage(current_init_lvl);
  }

  if(current_init_lvl == uacpi_kernel_init_state.current_level) {
    return UACPI_STATUS_OK;
  }

  if(current_init_lvl < uacpi_kernel_init_state.current_level) {
    serial_printf("uacpi_kernel_initialize: level regression from %s to %s\n",
                  uacpi_kernel_init_level_name(uacpi_kernel_init_state.current_level),
                  uacpi_kernel_init_level_name(current_init_lvl));
    return UACPI_STATUS_INIT_LEVEL_MISMATCH;
  }

  if(current_init_lvl != (uacpi_kernel_init_state.current_level + 1)) {
    serial_printf("uacpi_kernel_initialize: unexpected transition %s -> %s\n",
                  uacpi_kernel_init_level_name(uacpi_kernel_init_state.current_level),
                  uacpi_kernel_init_level_name(current_init_lvl));
    return UACPI_STATUS_INIT_LEVEL_MISMATCH;
  }

  uacpi_kernel_init_state.current_level = current_init_lvl;

  serial_printf("uacpi_kernel_initialize: level -> %s\n",
                uacpi_kernel_init_level_name(current_init_lvl));
  return uacpi_kernel_run_init_stage(current_init_lvl);
}

void uacpi_kernel_deinitialize(void) {
  uacpi_kernel_init_state.initialized = false;
  uacpi_kernel_init_state.current_level = UACPI_INIT_LEVEL_EARLY;

  serial_printf("uacpi_kernel_deinitialize: state reset\n");
}
