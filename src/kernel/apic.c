#include <kernel/apic.h>
#include <kernel/heap.h>
#include <kernel/idt.h>
#include <kernel/pmm.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>

#include <uacpi/acpi.h>
#include <uacpi/namespace.h>
#include <uacpi/tables.h>
#include <uacpi/utilities.h>

#include <stdint.h>
#include <stdio.h>

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
  if (uacpi_unlikely_error(ret)) {
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

	while (entry < listend) {
		if (entry->type == type)
			++count;
		entry = getnext(entry);
	}

	return count;
}

static inline void *getentry(int type, int n) {
	struct acpi_entry_hdr *entry = liststart;

	while (entry < listend) {
		if (entry->type == type) {
			if (n-- == 0)
				return entry;
		}
		entry = getnext(entry);
	}

	return NULL;
}

void apic_init() {
  printf("- Enabling APIC");

  uacpi_table tbl;

  uacpi_status ret = uacpi_table_find_by_signature("APIC", &tbl);
  if (uacpi_unlikely_error(ret)) {
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

  serial_printf("acpi_init: table @ %p\n", tbl.ptr);

  struct acpi_madt_lapic_address_override *lapic64 = getentry(ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE, 0);

	void *paddr = lapic64 ? (void *)lapic64->address : (void *)(uint64_t)madt->local_interrupt_controller_address;

	if (lapic64) {
		serial_printf("\e[94mUsing 64 bit override for the local APIC address\n\e[0m");
  }

  serial_printf("acpi_init: local APIC address: %p\n", paddr);

  lapicaddr = (void*)physical_to_virtual((uintptr_t)paddr);

  ioapics = (ioapicdesc_t*)kmalloc(sizeof(ioapicdesc_t) * iocount);

  serial_printf("acpi_init: iocount: %d\n", iocount);

  for (size_t i = 0; i < iocount; ++i) {
		struct acpi_madt_ioapic *entry = getentry(ACPI_MADT_ENTRY_TYPE_IOAPIC, i);

    ioapics[i].addr = (void*)physical_to_virtual((uintptr_t)entry->address);
    ioapics[i].top = ioapics[i].base + ((readioapic(ioapics[i].addr, IOAPIC_REG_ENTRYCOUNT) >> 16) & 0xff) + 1;
		serial_printf("ioapic%lu: addr %p base %lu top %lu\n", i, entry->address, entry->gsi_base, ioapics[i].top);
    for (int j = ioapics[i].base; j < ioapics[i].top; ++j) {
			writeiored(ioapics[i].addr, j - ioapics[i].base, 0xfe, 0, 0, 0, 0, 1, 0);
      puts(".");
    }
  }
  printf(".OK\n");
}

void write_lapic(uintptr_t reg, uint32_t value) {
  *((volatile uint32_t*) reg) = value;
}

uint32_t read_lapic(uintptr_t reg) {
  return *((volatile uint32_t*) reg);
}