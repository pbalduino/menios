#include <kernel/arch/x86_64/apic.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/console.h>
#include <kernel/heap.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>
#include <kernel/msr.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/namespace.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct ioapicdesc_t {
	void *addr;
	int base;
	int top;
} ioapicdesc_t;

static ioapicdesc_t *ioapics;

static struct acpi_madt *madt;
static struct acpi_entry_hdr *liststart;
static struct acpi_entry_hdr *listend;
static void *lapicaddr;

static bool lapic_x2apic_enabled = false;

static size_t overridecount;
static size_t iocount;
static size_t lapiccount;
static size_t lapicnmicount;

static uint32_t readioapic(void *ioapic, int reg) {
	volatile uint32_t *apic = ioapic;
	*apic = reg & 0xff;
	return *(apic + 4);
}

static void writeioapic(void *ioapic, int reg, uint32_t v) {
	volatile uint32_t *apic = ioapic;
	*apic = reg & 0xff;
	*(apic + 4) = v;
}

static void writeiored(void *ioapic, uint8_t entry, uint8_t vector, uint8_t delivery, uint8_t destmode, uint8_t polarity, uint8_t mode, uint8_t mask, uint8_t dest){
	uint32_t val = vector;
	val |= (delivery & 0b111) << 8;
	val |= (destmode & 1) << 11;
	val |= (polarity & 1) << 13;
	val |= (mode & 1) << 15;
	val |= (mask & 1) << 16;

	writeioapic(ioapic, 0x10 + entry * 2, val);
	writeioapic(ioapic, 0x11 + entry * 2, (uint32_t)dest << 24);
}

static uacpi_ns_iteration_decision acpi_init_one_device(void *ctx, uacpi_namespace_node *node){
  uacpi_namespace_node_info *info;

  uacpi_status ret = uacpi_get_namespace_node_info(node, &info);
  if(uacpi_unlikely_error(ret)) {
    const char *path = uacpi_namespace_node_generate_absolute_path(node);
    serial_printf("unable to retrieve node %s information: %s",
              path, uacpi_status_to_string(ret));
    uacpi_free_absolute_path(path);
    return UACPI_NS_ITERATION_DECISION_CONTINUE;
  }

  const uacpi_char* path = uacpi_namespace_node_generate_absolute_path(node);
  serial_printf("acpi_init_one_device: path: %s - name: %s\n", path, info->name.text);
  uacpi_free_absolute_path(path);

  uacpi_namespace_for_each_node_depth_first(node, acpi_init_one_device, UACPI_NULL);
  
  return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

static inline struct acpi_entry_hdr *getnext(struct acpi_entry_hdr *header) {
	uintptr_t ptr = (uintptr_t)header;
	return (struct acpi_entry_hdr *)(ptr + header->length);
}

static int getcount(int type) {
	struct acpi_entry_hdr *entry = liststart;
	int count = 0;

	while(entry < listend) {
		if(entry->type == type)
			++count;
		entry = getnext(entry);
	}

	return count;
}

static inline void *getentry(int type, int n) {
	struct acpi_entry_hdr *entry = liststart;

	while(entry < listend) {
		if(entry->type == type) {
			if(n-- == 0)
				return entry;
		}
		entry = getnext(entry);
	}

	return NULL;
}

void apic_initialize(void) {
  logk("Enabling APIC");

  uacpi_table tbl;

  uacpi_status ret = uacpi_table_find_by_signature("APIC", &tbl);
  if(uacpi_unlikely_error(ret)) {
    serial_printf("unable to find ACPI table: %s\n", uacpi_status_to_string(ret));
    return;
  }

	madt = tbl.ptr;
	liststart = (void *)((uintptr_t)madt + sizeof(struct acpi_madt));
	listend   = (void *)((uintptr_t)madt + madt->hdr.length);

	overridecount = getcount(ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE);
	iocount = getcount(ACPI_MADT_ENTRY_TYPE_IOAPIC);
	lapiccount = getcount(ACPI_MADT_ENTRY_TYPE_LAPIC);
	lapicnmicount = getcount(ACPI_MADT_ENTRY_TYPE_LAPIC_NMI);

  serial_printf("acpi_initialize: table @ %p\n", tbl.ptr);

  struct acpi_madt_lapic_address_override *lapic64 = getentry(ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE, 0);

	void *paddr = lapic64 ? (void *)lapic64->address : (void *)(uint64_t)madt->local_interrupt_controller_address;

  uint64_t apic_base = msr_read(LAPIC_BASE_MSR);
  lapic_x2apic_enabled = (apic_base & LAPIC_BASE_X2APIC_ENABLE) != 0;
  serial_printf("acpi_initialize: x2APIC %s\n", lapic_x2apic_enabled ? "enabled" : "disabled");

	if(lapic64) {
		serial_printf("\e[94mUsing 64 bit override for the local APIC address\n\e[0m");
  }

  serial_printf("acpi_initialize: local APIC address: %p\n", paddr);

  lapicaddr = (void*)physical_to_virtual((uintptr_t)paddr);

  ioapics = (ioapicdesc_t*)kmalloc(sizeof(ioapicdesc_t) * iocount);

  serial_printf("acpi_initialize: iocount: %d\n", iocount);

  for(size_t i = 0; i < iocount; ++i) {
		struct acpi_madt_ioapic *entry = getentry(ACPI_MADT_ENTRY_TYPE_IOAPIC, i);

    ioapics[i].addr = (void*)physical_to_virtual((uintptr_t)entry->address);
    ioapics[i].base = entry->gsi_base;
    ioapics[i].top = ioapics[i].base + ((readioapic(ioapics[i].addr, IOAPIC_REG_ENTRYCOUNT) >> 16) & 0xff) + 1;
		serial_printf("ioapic%lu: addr %p base %lu top %lu\n", i, entry->address, ioapics[i].base, ioapics[i].top);
    for(int j = ioapics[i].base; j < ioapics[i].top; ++j) {
			writeiored(ioapics[i].addr, j - ioapics[i].base, 0xfe, 0, 0, 0, 0, 1, 0);
      puts(".");
    }
  }
  irq_apic_online();
  printf(".OK\n");
}

static void pic_set_mask(uint32_t irq, bool masked) {
  if(irq >= 16) {
    return;
  }

  uint16_t port = (irq < 8) ? PIC1_DATA_PORT : PIC2_DATA_PORT;
  uint8_t bit = (uint8_t)(1u << (irq % 8));
  uint8_t value = inb(port);
  if(masked) {
    value |= bit;
  } else {
    value &= (uint8_t)~bit;
  }
  outb(port, value);
}

void write_lapic(uint32_t reg, uint32_t value) {
  if(lapic_x2apic_enabled) {
    uint32_t msr = IA32_X2APIC_BASE + (reg >> 4);
    msr_write(msr, (uint64_t)value);
    return;
  }

  if(lapicaddr == NULL) {
    return;
  }

  volatile uint32_t* target = (volatile uint32_t*)((uintptr_t)lapicaddr + reg);
  *target = value;
}

uint32_t read_lapic(uint32_t reg) {
  if(lapic_x2apic_enabled) {
    uint32_t msr = IA32_X2APIC_BASE + (reg >> 4);
    return (uint32_t)msr_read(msr);
  }

  if(lapicaddr == NULL) {
    return 0;
  }

  volatile uint32_t* target = (volatile uint32_t*)((uintptr_t)lapicaddr + reg);
  return *target;
}

static ioapicdesc_t* apic_find_ioapic(uint32_t gsi) {
  for(size_t i = 0; i < iocount; ++i) {
    if(gsi >= (uint32_t)ioapics[i].base && gsi < (uint32_t)ioapics[i].top) {
      return &ioapics[i];
    }
  }
  return NULL;
}

bool apic_configure_irq(uint32_t gsi,
                        uint8_t vector,
                        bool level_triggered,
                        bool active_low) {
  ioapicdesc_t* desc = apic_find_ioapic(gsi);
  if(desc == NULL) {
    serial_printf("apic_configure_irq: no IOAPIC covers GSI %u\n", gsi);
    return false;
  }

  uint8_t entry = (uint8_t)(gsi - (uint32_t)desc->base);
  uint8_t polarity = active_low ? 1 : 0;
  uint8_t trigger_mode = level_triggered ? 1 : 0;

  writeiored(desc->addr,
             entry,
             vector,
             0,            // delivery: fixed
             0,            // physical destination mode
             polarity,
             trigger_mode,
             0,            // unmask entry
             0);           // target CPU 0

  serial_printf("apic_configure_irq: GSI %u routed to vector 0x%02x (level=%s, active_low=%s)\n",
                gsi,
                (unsigned)vector,
                level_triggered ? "yes" : "no",
                active_low ? "yes" : "no");

  pic_set_mask(gsi, true);

  return true;
}

bool apic_update_irq_mask(uint32_t gsi, bool masked) {
  ioapicdesc_t* desc = apic_find_ioapic(gsi);
  if(desc == NULL) {
    if(gsi < 16) {
      pic_set_mask(gsi, masked);
      return true;
    }
    return false;
  }

  uint8_t entry = (uint8_t)(gsi - (uint32_t)desc->base);
  uint32_t value = readioapic(desc->addr, 0x10 + entry * 2);
  if(masked) {
    value |= (1u << 16);
  } else {
    value &= ~(1u << 16);
  }
  writeioapic(desc->addr, 0x10 + entry * 2, value);

  pic_set_mask(gsi, true);

  return true;
}

uint32_t apic_current_processor_id(void) {
  if(lapic_x2apic_enabled) {
    return (uint32_t)msr_read(IA32_X2APIC_APICID);
  }

  if(lapicaddr == NULL) {
    return 0;
  }

  volatile uint32_t* reg = (volatile uint32_t*)((uintptr_t)lapicaddr + 0x20);
  return (*reg >> 24) & 0xFFu;
}

void apic_send_eoi(void) {
  if(lapicaddr == NULL && !lapic_x2apic_enabled) {
    return;
  }

  write_lapic(LAPIC_EOI, 0);
}

void* apic_get_lapic_base(void) {
  return lapicaddr;
}

bool apic_is_x2apic_enabled(void) {
  return lapic_x2apic_enabled;
}
