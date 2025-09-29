#include <kernel/hpet.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#include <string.h>

#define FEMTOSECONDS_PER_SECOND 1000000000000000ull
#define HPET_ENABLE               (1u << 0)

static volatile uint8_t* hpet_mmio = NULL;
static uint64_t hpet_frequency;
static bool hpet_available;

static inline uint64_t hpet_read64(uint32_t offset) {
  return *((volatile uint64_t*)(hpet_mmio + offset));
}

static inline void hpet_write64(uint32_t offset, uint64_t value) {
  *((volatile uint64_t*)(hpet_mmio + offset)) = value;
}

static inline void hpet_disable(void) {
  uint64_t cfg = hpet_read64(HPET_REG_CONFIGURATION);
  cfg &= ~HPET_ENABLE;
  hpet_write64(HPET_REG_CONFIGURATION, cfg);
}

static inline void hpet_enable(void) {
  uint64_t cfg = hpet_read64(HPET_REG_CONFIGURATION);
  cfg |= HPET_ENABLE;
  hpet_write64(HPET_REG_CONFIGURATION, cfg);
}

hpet_status_t hpet_timer_init(void) {
  if(hpet_available) {
    return HPET_OK;
  }

  uacpi_table tbl;

  uacpi_status ret = uacpi_table_find_by_signature("HPET", &tbl);
  if(uacpi_unlikely_error(ret)) {
    serial_printf("hpet_timer_init: unable to find HPET table: %s\n",
                  uacpi_status_to_string(ret));
    return HPET_ERROR;
  }

  hpet_table_t* table = (hpet_table_t*)tbl.virt_addr;
  if(table == NULL) {
    serial_printf("hpet_timer_init: HPET table pointer invalid\n");
    return HPET_ERROR;
  }

  if(table->address.space_id != ACPI_SYSTEM_MEMORY) {
    serial_printf("hpet_timer_init: HPET not mapped in system memory (space=%u)\n",
                  table->address.space_id);
    return HPET_ERROR;
  }

  phys_addr_t base_phys = table->address.address;
  if(base_phys == 0) {
    serial_printf("hpet_timer_init: HPET base address is zero\n");
    return HPET_ERROR;
  }

  hpet_mmio = (volatile uint8_t*)physical_to_virtual(base_phys);
  if(hpet_mmio == NULL) {
    serial_printf("hpet_timer_init: failed to map HPET base %lx\n",
                  (unsigned long)base_phys);
    return HPET_ERROR;
  }

  hpet_disable();

  uint64_t capabilities = hpet_read64(HPET_REG_CAPABILITIES);
  uint64_t period_fs = capabilities >> 32;
  if(period_fs == 0) {
    serial_printf("hpet_timer_init: invalid clock period\n");
    return HPET_ERROR;
  }

  hpet_frequency = FEMTOSECONDS_PER_SECOND / period_fs;
  if(hpet_frequency == 0) {
    serial_printf("hpet_timer_init: computed clock frequency is zero\n");
    return HPET_ERROR;
  }

  hpet_write64(HPET_REG_MAIN_COUNTER, 0);
  hpet_enable();

  hpet_available = true;
  serial_printf("Found HPET @ %p (freq=%llu Hz)\n",
                (void*)base_phys,
                (unsigned long long)hpet_frequency);

  return HPET_OK;
}

bool hpet_is_available(void) {
  return hpet_available;
}

uint64_t hpet_frequency_hz(void) {
  return hpet_frequency;
}

uint64_t hpet_read_counter(void) {
  if(!hpet_available) {
    return 0;
  }
  return hpet_read64(HPET_REG_MAIN_COUNTER);
}

void hpet_reset_counter(void) {
  if(!hpet_available) {
    return;
  }
  hpet_write64(HPET_REG_MAIN_COUNTER, 0);
}

void hpet_wait_ticks(uint64_t ticks) {
  if(!hpet_available || ticks == 0) {
    return;
  }
  uint64_t start = hpet_read_counter();
  while((hpet_read_counter() - start) < ticks) {
    __asm__ volatile("pause");
  }
}

void hpet_wait_us(uint64_t microseconds) {
  if(!hpet_available || microseconds == 0) {
    return;
  }
  uint64_t ticks = (hpet_frequency * microseconds) / 1000000ull;
  if(ticks == 0) {
    ticks = 1;
  }
  hpet_wait_ticks(ticks);
}
