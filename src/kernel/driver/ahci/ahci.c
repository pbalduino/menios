#include <kernel/ahci.h>
#include <kernel/apic.h>
#include <kernel/block_device.h>
#include <kernel/dma.h>
#include <kernel/heap.h>
#include <kernel/idt.h>
#include <kernel/mutex.h>
#include <kernel/pci.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <kernel/spinlock.h>
#include <kernel/tsc.h>

#include <stdio.h>
#include <string.h>

#ifndef AHCI_VERBOSE_LOG
#define AHCI_VERBOSE_LOG 1
#endif

#define PCI_HEADER_TYPE_MULTIFUNC 0x80

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROGIF_AHCI        0x01

#define PCI_CONFIG_VENDOR_DEVICE  0x00
#define PCI_CONFIG_STATUS_COMMAND 0x04
#define PCI_CONFIG_CLASSREV       0x08
#define PCI_CONFIG_BAR5           0x24
#define PCI_CONFIG_HEADER_TYPE    0x0C
#define PCI_CONFIG_INTERRUPT_LINE 0x3C

#define PCI_COMMAND_MEMORY_SPACE (1u << 1)
#define PCI_COMMAND_BUS_MASTER   (1u << 2)

#define AHCI_INVALID_GSI 0xFFFFFFFFu
#define AHCI_GHC_IE      (1u << 1)
#define AHCI_GHC_AE      (1u << 31)

#define AHCI_MAX_PORTS          32
#define AHCI_CMD_SLOT           0
#define AHCI_CMD_SLOT_MASK      (1u << AHCI_CMD_SLOT)
#define AHCI_SECTOR_SIZE        512u
#define AHCI_MAX_PRDT_ENTRIES     8u
#define AHCI_PRDT_MAX_BYTES       (4u * 1024u * 1024u)
#define AHCI_BOUNCE_BUFFER_BYTES  (512u * 1024u)
#define AHCI_READY_TIMEOUT_NS   (500ull * 1000ull * 1000ull)
#define AHCI_COMMAND_TIMEOUT_NS (5ull * 1000ull * 1000ull * 1000ull)

#define HBA_PxCMD_ST   (1u << 0)
#define HBA_PxCMD_FRE  (1u << 4)
#define HBA_PxCMD_FR   (1u << 14)
#define HBA_PxCMD_CR   (1u << 15)

#define HBA_PxIS_DHRS  (1u << 0)
#define HBA_PxIS_PSS   (1u << 1)
#define HBA_PxIS_SDBS  (1u << 3)
#define HBA_PxIS_UFS   (1u << 4)
#define HBA_PxIS_TFES  (1u << 30)
#define HBA_PxIS_CPDS  (1u << 31)
#define HBA_PxIS_ALL   (HBA_PxIS_DHRS | HBA_PxIS_PSS | HBA_PxIS_SDBS | HBA_PxIS_UFS | HBA_PxIS_TFES | HBA_PxIS_CPDS)

#define AHCI_PORT_IRQ_MASK (HBA_PxIS_DHRS | HBA_PxIS_SDBS | HBA_PxIS_TFES | HBA_PxIS_CPDS)

#define HBA_PxSSTS_DET_PRESENT 0x03u
#define HBA_PxSSTS_IPM_ACTIVE  0x01u

#define SATA_SIG_ATA    0x00000101u
#define SATA_SIG_ATAPI  0xEB140101u
#define SATA_SIG_SEMB   0xC33C0101u
#define SATA_SIG_PM     0x96690101u

#define ATA_CMD_IDENTIFY     0xECu
#define ATA_CMD_READ_DMA_EXT 0x25u
#define ATA_CMD_WRITE_DMA_EXT 0x35u

#define ATA_STATUS_ERR 0x01u
#define ATA_STATUS_DRQ 0x08u
#define ATA_STATUS_BSY 0x80u

#define AHCI_CMD_FLAG_CFL(n)   ((uint16_t)((n) & 0x1Fu))
#define AHCI_CMD_FLAG_A        (1u << 5)
#define AHCI_CMD_FLAG_W        (1u << 6)
#define AHCI_CMD_FLAG_P        (1u << 7)
#define AHCI_CMD_FLAG_R        (1u << 8)
#define AHCI_CMD_FLAG_B        (1u << 9)
#define AHCI_CMD_FLAG_C        (1u << 10)
#define AHCI_CMD_FLAG_PMP(p)   ((uint16_t)(((p) & 0xFu) << 12))

#define AHCI_CMD_FIS_LENGTH_DWORDS 5u

typedef struct ahci_hba_port {
  uint32_t clb;
  uint32_t clbu;
  uint32_t fb;
  uint32_t fbu;
  uint32_t is;
  uint32_t ie;
  uint32_t cmd;
  uint32_t reserved0;
  uint32_t tfd;
  uint32_t sig;
  uint32_t ssts;
  uint32_t sctl;
  uint32_t serr;
  uint32_t sact;
  uint32_t ci;
  uint32_t sntf;
  uint32_t fbs;
  uint32_t reserved1[11];
  uint32_t vendor[4];
} __attribute__((packed)) ahci_hba_port_t;

typedef struct ahci_hba_mem {
  uint32_t cap;
  uint32_t ghc;
  uint32_t is;
  uint32_t pi;
  uint32_t vs;
  uint32_t ccc_ctl;
  uint32_t ccc_pts;
  uint32_t em_loc;
  uint32_t em_ctl;
  uint32_t cap2;
  uint32_t bohc;
  uint8_t  reserved[0x74];
  uint8_t  vendor[0x60];
  ahci_hba_port_t ports[AHCI_MAX_PORTS];
} __attribute__((packed)) ahci_hba_mem_t;

typedef struct ahci_prdt_entry {
  uint32_t dba;
  uint32_t dbau;
  uint32_t reserved;
  uint32_t dbc;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct ahci_command_header {
  uint16_t flags;
  uint16_t prdtl;
  uint32_t prdbc;
  uint32_t ctba;
  uint32_t ctbau;
  uint32_t reserved[4];
} __attribute__((packed)) ahci_command_header_t;

typedef struct ahci_command_table {
  uint8_t            cfis[64];
  uint8_t            acmd[16];
  uint8_t            reserved[48];
  ahci_prdt_entry_t  prdt[AHCI_MAX_PRDT_ENTRIES];
} __attribute__((packed)) ahci_command_table_t;

typedef enum {
  AHCI_PORT_STATE_EMPTY = 0,
  AHCI_PORT_STATE_ATTACHED,
  AHCI_PORT_STATE_ONLINE
} ahci_port_state_t;

typedef struct ahci_transfer_context {
  uint64_t   lba;
  uint32_t   sector_count;
  void*      buffer;
  size_t     byte_count;
  bool       write;
  phys_addr_t dma_phys;
} ahci_transfer_context_t;

struct ahci_port_t {
  ahci_controller_t*      controller;
  uint8_t                 index;
  ahci_port_state_t       state;
  volatile ahci_hba_port_t* regs;
  dma_buffer_t            cmd_list;
  dma_buffer_t            fis;
  dma_buffer_t            cmd_tables;
  dma_buffer_t            bounce;
  uint32_t                block_size;
  uint64_t                block_count;
  block_device_t          device;
  kmutex_t                lock;
  bool                    registered;
};

typedef struct ahci_port_t ahci_port_t;

static ahci_controller_t* controllers_head = NULL;
static size_t controllers_count = 0;
static bool ahci_initialized = false;

static const block_device_ops_t ahci_block_ops;

static bool ahci_is_candidate(uint8_t bus, uint8_t device, uint8_t function);
static void ahci_enable_memory_and_busmaster(uint8_t bus, uint8_t device, uint8_t function);
static void ahci_controller_configure(ahci_controller_t* controller);
static bool ahci_controller_enable_interrupts(ahci_controller_t* controller);
static void ahci_controller_discover_ports(ahci_controller_t* controller);
static void ahci_port_init(ahci_port_t* port, ahci_controller_t* controller, uint8_t index);
static bool ahci_port_configure_dma(ahci_port_t* port);
static void ahci_port_shutdown_dma(ahci_port_t* port);
static bool ahci_port_device_present(ahci_port_t* port);
static bool ahci_port_start(ahci_port_t* port);
static void ahci_port_stop(ahci_port_t* port);
static bool ahci_port_identify(ahci_port_t* port);
static bool ahci_port_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count);
static bool ahci_port_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count);
static bool ahci_port_flush(block_device_t* device);
static bool ahci_port_transfer(ahci_port_t* port, uint64_t lba, void* buffer, size_t block_count, bool write);
static bool ahci_port_issue_command(ahci_port_t* port,
                                    const ahci_transfer_context_t* ctx,
                                    uint8_t command);
static bool ahci_port_wait_ready(ahci_port_t* port, uint64_t timeout_ns);
static bool ahci_port_wait_complete(ahci_port_t* port, uint32_t slot_mask, uint64_t timeout_ns, bool* fatal_error);
static void ahci_port_register_device(ahci_port_t* port, uint32_t device_index);

static const block_device_ops_t ahci_block_ops = {
  .read_blocks = ahci_port_read,
  .write_blocks = ahci_port_write,
  .flush = ahci_port_flush,
};

static bool ahci_is_candidate(uint8_t bus, uint8_t device, uint8_t function) {
  uint32_t class_reg = pci_config_read(bus, device, function, PCI_CONFIG_CLASSREV);
  uint8_t class_code = (class_reg >> 24) & 0xFF;
  uint8_t subclass = (class_reg >> 16) & 0xFF;
  uint8_t prog_if = (class_reg >> 8) & 0xFF;

  return class_code == PCI_CLASS_MASS_STORAGE &&
         subclass == PCI_SUBCLASS_SATA &&
         (prog_if & 0x80 ? (prog_if & 0x7F) == PCI_PROGIF_AHCI : prog_if == PCI_PROGIF_AHCI);
}

static void ahci_enable_memory_and_busmaster(uint8_t bus, uint8_t device, uint8_t function) {
  uint32_t command = pci_config_read(bus, device, function, PCI_CONFIG_STATUS_COMMAND);
  command |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
  pci_config_write(bus, device, function, PCI_CONFIG_STATUS_COMMAND, command);
}

static void ahci_register_controller(uint8_t bus,
                                     uint8_t device,
                                     uint8_t function,
                                     phys_addr_t abar_phys,
                                     uint8_t irq_line,
                                     uint8_t irq_pin) {
  ahci_controller_t* node = kmalloc(sizeof(*node));
  if(node == NULL) {
    serial_printf("ahci: failed to allocate controller descriptor for %02x:%02x.%u\n",
                  bus,
                  device,
                  function);
    return;
  }

  memset(node, 0, sizeof(*node));
  node->bus = bus;
  node->device = device;
  node->function = function;
  node->abar_phys = abar_phys;
  node->abar = (void*)physical_to_virtual(abar_phys);
  node->irq_line = irq_line;
  node->irq_pin = irq_pin;
  node->gsi = (irq_line == 0xFF) ? AHCI_INVALID_GSI : (uint32_t)irq_line;
  node->irq_configured = false;
  node->ports = NULL;
  node->port_count = 0;
  node->next = controllers_head;
  controllers_head = node;
  controllers_count++;

#if AHCI_VERBOSE_LOG
  serial_printf("ahci: controller %02x:%02x.%u mapped at phys=%llx virt=%p\n",
                bus,
                device,
                function,
                (unsigned long long)abar_phys,
                node->abar);
#endif

  ahci_controller_configure(node);
}

static bool ahci_controller_enable_interrupts(ahci_controller_t* controller) {
  if(controller->gsi == AHCI_INVALID_GSI) {
#if AHCI_VERBOSE_LOG
    serial_printf("ahci: controller %02x:%02x.%u has no valid IRQ line (pin=%u)\n",
                  controller->bus,
                  controller->device,
                  controller->function,
                  controller->irq_pin);
#endif
    return false;
  }

  if(!controller->irq_configured) {
    if(!apic_configure_irq(controller->gsi, ISR_AHCI, true, true)) {
      serial_printf("ahci: controller %02x:%02x.%u failed to route IRQ (GSI %u)\n",
                    controller->bus,
                    controller->device,
                    controller->function,
                    controller->gsi);
      return false;
    }
    controller->irq_configured = true;
  }

  volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)controller->abar;
  uint32_t ghc = hba->ghc;
  ghc |= AHCI_GHC_IE | AHCI_GHC_AE;
  hba->ghc = ghc;

#if AHCI_VERBOSE_LOG
  serial_printf("ahci: controller %02x:%02x.%u interrupts routed on vector 0x%02x (GSI %u)\n",
                controller->bus,
                controller->device,
                controller->function,
                ISR_AHCI,
                controller->gsi);
#endif
  return true;
}

static void ahci_controller_configure(ahci_controller_t* controller) {
  if(controller == NULL || controller->abar == NULL) {
    return;
  }

  ahci_controller_enable_interrupts(controller);
  ahci_controller_discover_ports(controller);
}

static void ahci_controller_discover_ports(ahci_controller_t* controller) {
  volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)controller->abar;
  uint32_t implemented = hba->pi;

  if(controller->ports != NULL) {
    return;
  }

  ahci_port_t* ports = kmalloc(sizeof(ahci_port_t) * AHCI_MAX_PORTS);
  if(ports == NULL) {
    serial_printf("ahci: unable to allocate port table for controller %02x:%02x.%u\n",
                  controller->bus,
                  controller->device,
                  controller->function);
    return;
  }

  memset(ports, 0, sizeof(ahci_port_t) * AHCI_MAX_PORTS);

  controller->ports = ports;
  controller->port_count = 0;

  for(uint32_t port_index = 0; port_index < AHCI_MAX_PORTS; port_index++) {
    if(((implemented >> port_index) & 0x1u) == 0) {
      continue;
    }

    ahci_port_t* port = &ports[port_index];
    ahci_port_init(port, controller, (uint8_t)port_index);

    if(port->state == AHCI_PORT_STATE_ONLINE) {
      controller->port_count++;
    }
  }
}

static void ahci_port_init(ahci_port_t* port, ahci_controller_t* controller, uint8_t index) {
  memset(port, 0, sizeof(*port));
  port->controller = controller;
  port->index = index;
  port->state = AHCI_PORT_STATE_EMPTY;
  port->regs = &((volatile ahci_hba_mem_t*)controller->abar)->ports[index];
  kmutex_init(&port->lock);

  if(!ahci_port_device_present(port)) {
#if AHCI_VERBOSE_LOG
    serial_printf("ahci: ctrl %02x:%02x.%u port %u no device detected (ssts=0x%08x)\n",
                  controller->bus,
                  controller->device,
                  controller->function,
                  index,
                  port->regs->ssts);
#endif
    return;
  }

  if(!ahci_port_configure_dma(port)) {
    ahci_port_shutdown_dma(port);
    return;
  }

  if(!ahci_port_start(port)) {
    ahci_port_shutdown_dma(port);
    return;
  }

  if(!ahci_port_identify(port)) {
    ahci_port_stop(port);
    ahci_port_shutdown_dma(port);
    return;
  }

  ahci_port_register_device(port, controller->port_count);
  port->state = AHCI_PORT_STATE_ONLINE;
}

static bool ahci_port_device_present(ahci_port_t* port) {
  uint32_t ssts = port->regs->ssts;
  uint32_t det = ssts & 0x0Fu;
  uint32_t ipm = (ssts >> 8) & 0x0Fu;

  if(det != HBA_PxSSTS_DET_PRESENT || ipm != HBA_PxSSTS_IPM_ACTIVE) {
    return false;
  }

  uint32_t sig = port->regs->sig;
  if(sig == SATA_SIG_ATAPI || sig == SATA_SIG_PM || sig == SATA_SIG_SEMB) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u unsupported device signature 0x%08x\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index,
                  sig);
    return false;
  }

  return sig == SATA_SIG_ATA;
}

static void ahci_port_clear_interrupts(ahci_port_t* port) {
  volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)port->controller->abar;
  uint32_t pending = port->regs->is;
  if(pending != 0) {
    port->regs->is = pending;
  }
  hba->is = (1u << port->index);
}

static void ahci_port_stop(ahci_port_t* port) {
  volatile ahci_hba_port_t* regs = port->regs;

  uint32_t cmd = regs->cmd;
  cmd &= ~HBA_PxCMD_ST;
  regs->cmd = cmd;

  while(regs->cmd & HBA_PxCMD_CR) {
    asm volatile("pause");
  }

  cmd = regs->cmd;
  cmd &= ~HBA_PxCMD_FRE;
  regs->cmd = cmd;

  while(regs->cmd & HBA_PxCMD_FR) {
    asm volatile("pause");
  }
}

static bool ahci_port_configure_dma(ahci_port_t* port) {
  if(!dma_alloc_default(&port->cmd_list, sizeof(ahci_command_header_t) * 32)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u failed to allocate command list\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  if(!dma_buffer_alloc(&port->fis, 256, 256, DMA_DEFAULT_MAX_PHYS, true)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u failed to allocate FIS buffer\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  size_t table_size = sizeof(ahci_command_table_t);
  size_t total_table_bytes = table_size * 32;
  if(!dma_buffer_alloc(&port->cmd_tables, total_table_bytes, 128, DMA_DEFAULT_MAX_PHYS, true)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u failed to allocate command tables\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  if(!dma_buffer_alloc(&port->bounce,
                       AHCI_BOUNCE_BUFFER_BYTES,
                       DMA_DEFAULT_ALIGNMENT,
                       DMA_DEFAULT_MAX_PHYS,
                       true)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u failed to allocate bounce buffer\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  volatile ahci_hba_port_t* regs = port->regs;

  ahci_port_stop(port);

  regs->clb  = (uint32_t)(port->cmd_list.phys & 0xFFFFFFFFull);
  regs->clbu = (uint32_t)(port->cmd_list.phys >> 32);
  regs->fb   = (uint32_t)(port->fis.phys & 0xFFFFFFFFull);
  regs->fbu  = (uint32_t)(port->fis.phys >> 32);

  memset((void*)port->cmd_list.virt, 0, port->cmd_list.size);
  memset((void*)port->fis.virt, 0, port->fis.size);
  memset((void*)port->cmd_tables.virt, 0, port->cmd_tables.size);

  regs->ie = AHCI_PORT_IRQ_MASK;
  regs->serr = 0xFFFFFFFFu;
  ahci_port_clear_interrupts(port);

  return true;
}

static void ahci_port_shutdown_dma(ahci_port_t* port) {
  if(port->cmd_tables.virt) {
    dma_buffer_free(&port->cmd_tables);
  }
  if(port->bounce.virt) {
    dma_buffer_free(&port->bounce);
  }
  if(port->fis.virt) {
    dma_buffer_free(&port->fis);
  }
  if(port->cmd_list.virt) {
    dma_buffer_free(&port->cmd_list);
  }
}

static bool ahci_port_start(ahci_port_t* port) {
  volatile ahci_hba_port_t* regs = port->regs;
  uint32_t cmd = regs->cmd;

  cmd |= HBA_PxCMD_FRE;
  regs->cmd = cmd;

  cmd |= HBA_PxCMD_ST;
  regs->cmd = cmd;

  return true;
}

static bool ahci_port_wait_ready(ahci_port_t* port, uint64_t timeout_ns) {
  useconds_t start = ns_from_boot();
  volatile ahci_hba_port_t* regs = port->regs;

  while((regs->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) != 0) {
    if(((uint64_t)ns_from_boot() - (uint64_t)start) > timeout_ns) {
      return false;
    }
    asm volatile("pause");
  }

  return true;
}

static ahci_command_header_t* ahci_port_command_header(ahci_port_t* port, uint32_t slot) {
  return &((ahci_command_header_t*)port->cmd_list.virt)[slot];
}

static ahci_command_table_t* ahci_port_command_table(ahci_port_t* port, uint32_t slot) {
  size_t table_size = sizeof(ahci_command_table_t);
  uint8_t* base = (uint8_t*)port->cmd_tables.virt;
  return (ahci_command_table_t*)(base + (table_size * slot));
}

static void ahci_fill_fis_command(ahci_command_table_t* table,
                                  const ahci_transfer_context_t* ctx,
                                  uint8_t command) {
  memset(table->cfis, 0, sizeof(table->cfis));

  table->cfis[0] = 0x27;            // Register – Host to Device FIS
  table->cfis[1] = (1u << 7);       // Command
  table->cfis[2] = command;
  table->cfis[3] = 0;

  uint64_t lba = ctx->lba;
  table->cfis[4] = (uint8_t)(lba & 0xFFu);
  table->cfis[5] = (uint8_t)((lba >> 8) & 0xFFu);
  table->cfis[6] = (uint8_t)((lba >> 16) & 0xFFu);
  table->cfis[7] = (uint8_t)(0x40u);  // LBA mode
  table->cfis[8] = (uint8_t)((lba >> 24) & 0xFFu);
  table->cfis[9] = (uint8_t)((lba >> 32) & 0xFFu);
  table->cfis[10] = (uint8_t)((lba >> 40) & 0xFFu);

  uint16_t count = (uint16_t)ctx->sector_count;
  table->cfis[12] = (uint8_t)(count & 0xFFu);
  table->cfis[13] = (uint8_t)((count >> 8) & 0xFFu);
}

static bool ahci_port_issue_command(ahci_port_t* port,
                                    const ahci_transfer_context_t* ctx,
                                    uint8_t command) {
  if(!ahci_port_wait_ready(port, AHCI_READY_TIMEOUT_NS)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u timeout waiting for device ready\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  ahci_command_header_t* header = ahci_port_command_header(port, AHCI_CMD_SLOT);
  ahci_command_table_t* table = ahci_port_command_table(port, AHCI_CMD_SLOT);

  memset(header, 0, sizeof(*header));
  memset(table, 0, sizeof(*table));
  ahci_fill_fis_command(table, ctx, command);

  header->flags = AHCI_CMD_FLAG_CFL(AHCI_CMD_FIS_LENGTH_DWORDS) | AHCI_CMD_FLAG_PMP(0);
  if(ctx->write) {
    header->flags |= AHCI_CMD_FLAG_W;
  }
  header->prdtl = 1;
  header->prdbc = 0;
  size_t table_offset = sizeof(ahci_command_table_t) * AHCI_CMD_SLOT;
  phys_addr_t table_phys = port->cmd_tables.phys + table_offset;
  header->ctba = (uint32_t)(table_phys & 0xFFFFFFFFull);
  header->ctbau = (uint32_t)(table_phys >> 32);

  ahci_prdt_entry_t* prdt = &table->prdt[0];
  prdt->dba = (uint32_t)(ctx->dma_phys & 0xFFFFFFFFull);
  prdt->dbau = (uint32_t)(ctx->dma_phys >> 32);
  prdt->dbc = (uint32_t)(ctx->byte_count - 1) | (1u << 31);
  prdt->reserved = 0;

  volatile ahci_hba_port_t* regs = port->regs;
  regs->ci = 0;
  ahci_port_clear_interrupts(port);

  regs->ci = AHCI_CMD_SLOT_MASK;

  bool fatal_error = false;
  bool completed = ahci_port_wait_complete(port, AHCI_CMD_SLOT_MASK, AHCI_COMMAND_TIMEOUT_NS, &fatal_error);

  if(!completed || fatal_error) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u command 0x%02x failed (ci=0x%08x is=0x%08x tfd=0x%08x)\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index,
                  command,
                  regs->ci,
                  regs->is,
                  regs->tfd);
    return false;
  }

  return true;
}

static bool ahci_port_wait_complete(ahci_port_t* port,
                                    uint32_t slot_mask,
                                    uint64_t timeout_ns,
                                    bool* fatal_error) {
  volatile ahci_hba_port_t* regs = port->regs;
  volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)port->controller->abar;
  useconds_t start = ns_from_boot();

  if(fatal_error) {
    *fatal_error = false;
  }

  while(true) {
    uint32_t ci = regs->ci;
    uint32_t is = regs->is;

    if((ci & slot_mask) == 0) {
      if(is != 0) {
        regs->is = is;
      }
      hba->is = (1u << port->index);

      if(is & HBA_PxIS_TFES) {
        if(fatal_error) {
          *fatal_error = true;
        }
        regs->serr = regs->serr;
        return false;
      }

      if(regs->tfd & ATA_STATUS_ERR) {
        return false;
      }

      return true;
    }

    if(is & HBA_PxIS_TFES) {
      regs->is = is;
      hba->is = (1u << port->index);
      regs->serr = regs->serr;
      if(fatal_error) {
        *fatal_error = true;
      }
      return false;
    }

    if(((uint64_t)ns_from_boot() - (uint64_t)start) > timeout_ns) {
      if(fatal_error) {
        *fatal_error = true;
      }
      return false;
    }

    asm volatile("pause");
  }
}

static bool ahci_port_identify(ahci_port_t* port) {
  dma_buffer_t identify_buffer;
  if(!dma_alloc_default(&identify_buffer, AHCI_SECTOR_SIZE)) {
    serial_printf("ahci: ctrl %02x:%02x.%u port %u failed to allocate identify buffer\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
    return false;
  }

  ahci_transfer_context_t ctx = {
    .lba = 0,
    .sector_count = 1,
    .buffer = identify_buffer.virt,
    .byte_count = AHCI_SECTOR_SIZE,
    .write = false,
  };

  bool ok = ahci_port_issue_command(port, &ctx, ATA_CMD_IDENTIFY);
  if(!ok) {
    dma_buffer_free(&identify_buffer);
    return false;
  }

  uint16_t* identify = (uint16_t*)identify_buffer.virt;
  uint64_t sectors_48 = ((uint64_t)identify[103] << 48) |
                        ((uint64_t)identify[102] << 32) |
                        ((uint64_t)identify[101] << 16) |
                        (uint64_t)identify[100];

  if(sectors_48 == 0) {
    uint64_t sectors_28 = ((uint64_t)identify[61] << 16) | (uint64_t)identify[60];
    sectors_48 = sectors_28;
  }

  port->block_size = AHCI_SECTOR_SIZE;
  port->block_count = sectors_48;

  dma_buffer_free(&identify_buffer);
  return port->block_count != 0;
}

static void ahci_port_register_device(ahci_port_t* port, uint32_t device_index) {
  const char prefix[] = "sata";
  size_t prefix_len = sizeof(prefix) - 1;
  size_t pos = 0;

  memset(port->device.name, 0, sizeof(port->device.name));

  while(pos < prefix_len && pos < sizeof(port->device.name) - 1) {
    port->device.name[pos] = prefix[pos];
    pos++;
  }

  uint32_t value = device_index;
  char digits[16];
  size_t digit_count = 0;

  if(value == 0) {
    digits[digit_count++] = '0';
  } else {
    while(value > 0 && digit_count < sizeof(digits)) {
      digits[digit_count++] = (char)('0' + (value % 10u));
      value /= 10u;
    }
  }

  while(digit_count > 0 && pos < sizeof(port->device.name) - 1) {
    port->device.name[pos++] = digits[--digit_count];
  }

  port->device.name[pos] = '\0';
  port->device.block_size = port->block_size;
  port->device.block_count = port->block_count;
  port->device.ops = &ahci_block_ops;
  port->device.driver_ctx = port;
  port->device.next = NULL;

  if(block_device_register(&port->device)) {
    port->registered = true;
    serial_printf("ahci: registered block device '%s' (%llu MiB)\n",
                  port->device.name,
                  (unsigned long long)((port->block_count * (uint64_t)port->block_size) / (1024ull * 1024ull)));
  } else {
    serial_printf("ahci: failed to register block device for ctrl %02x:%02x.%u port %u\n",
                  port->controller->bus,
                  port->controller->device,
                  port->controller->function,
                  port->index);
  }
}

static bool ahci_port_transfer(ahci_port_t* port, uint64_t lba, void* buffer, size_t block_count, bool write) {
  if(block_count == 0 || buffer == NULL || port->bounce.virt == NULL) {
    return false;
  }

  size_t max_chunk_sectors = port->bounce.size / AHCI_SECTOR_SIZE;
  if(max_chunk_sectors == 0) {
    return false;
  }

  size_t sectors_remaining = block_count;
  uint64_t current_lba = lba;
  uint8_t* user_ptr = (uint8_t*)buffer;

  while(sectors_remaining > 0) {
    size_t chunk_sectors = sectors_remaining > max_chunk_sectors ? max_chunk_sectors : sectors_remaining;
    size_t chunk_bytes = chunk_sectors * AHCI_SECTOR_SIZE;

    if(write) {
      memcpy(port->bounce.virt, user_ptr, chunk_bytes);
    }

    ahci_transfer_context_t tmp_ctx = {
      .lba = current_lba,
      .sector_count = (uint32_t)chunk_sectors,
      .buffer = port->bounce.virt,
      .byte_count = chunk_bytes,
      .write = write,
      .dma_phys = port->bounce.phys,
    };

    uint8_t command = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    if(!ahci_port_issue_command(port, &tmp_ctx, command)) {
      return false;
    }

    if(!write) {
      memcpy(user_ptr, port->bounce.virt, chunk_bytes);
    }

    user_ptr += chunk_bytes;
    current_lba += chunk_sectors;
    sectors_remaining -= chunk_sectors;
  }

  return true;
}

static bool ahci_port_read(block_device_t* device, uint64_t lba, void* buffer, size_t block_count) {
  if(device == NULL || buffer == NULL || block_count == 0) {
    return false;
  }

  ahci_port_t* port = (ahci_port_t*)device->driver_ctx;
  if(port == NULL) {
    return false;
  }

  kmutex_lock(&port->lock);
  bool result = ahci_port_transfer(port, lba, buffer, block_count, false);
  kmutex_unlock(&port->lock);
  return result;
}

static bool ahci_port_write(block_device_t* device, uint64_t lba, const void* buffer, size_t block_count) {
  if(device == NULL || buffer == NULL || block_count == 0) {
    return false;
  }

  ahci_port_t* port = (ahci_port_t*)device->driver_ctx;
  if(port == NULL) {
    return false;
  }

  kmutex_lock(&port->lock);
  bool result = ahci_port_transfer(port, lba, (void*)buffer, block_count, true);
  kmutex_unlock(&port->lock);
  return result;
}

static bool ahci_port_flush(block_device_t* device) {
  (void)device;
  return true;
}

static void ahci_scan_bus(void) {
  for(uint16_t bus = 0; bus < 256; bus++) {
    for(uint16_t device = 0; device < 32; device++) {
      uint8_t pci_bus = (uint8_t)bus;
      uint8_t pci_device = (uint8_t)device;
      uint32_t vendor_device = pci_config_read(pci_bus, pci_device, 0, PCI_CONFIG_VENDOR_DEVICE);
      if((vendor_device & 0xFFFF) == 0xFFFF) {
        continue;
      }

      uint8_t header_type = (pci_config_read(pci_bus, pci_device, 0, PCI_CONFIG_HEADER_TYPE) >> 16) & 0xFF;
      uint8_t function_limit = (header_type & PCI_HEADER_TYPE_MULTIFUNC) ? 8 : 1;

      for(uint16_t function = 0; function < function_limit; function++) {
        uint8_t pci_function = (uint8_t)function;
        vendor_device = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_VENDOR_DEVICE);
        if((vendor_device & 0xFFFF) == 0xFFFF) {
          continue;
        }

        if(!ahci_is_candidate(pci_bus, pci_device, pci_function)) {
          continue;
        }

        uint32_t bar5 = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_BAR5);
        if((bar5 & 0xFFFFFFF0u) == 0) {
#if AHCI_VERBOSE_LOG
          serial_printf("ahci: controller %02x:%02x.%u missing BAR5\n", pci_bus, pci_device, pci_function);
#endif
          continue;
        }

        uint32_t intr_line = pci_config_read(pci_bus, pci_device, pci_function, PCI_CONFIG_INTERRUPT_LINE);
        uint8_t irq_line = (uint8_t)(intr_line & 0xFF);
        uint8_t irq_pin = (uint8_t)((intr_line >> 8) & 0xFF);

        phys_addr_t abar_phys = (phys_addr_t)(bar5 & ~0xFu);
        ahci_enable_memory_and_busmaster(pci_bus, pci_device, pci_function);
        ahci_register_controller(pci_bus, pci_device, pci_function, abar_phys, irq_line, irq_pin);
      }
    }
  }
}

void ahci_irq_handler(void) {
  bool serviced = false;

  for(ahci_controller_t* controller = controllers_head; controller != NULL; controller = controller->next) {
    if(!controller->irq_configured || controller->abar == NULL) {
      continue;
    }

    volatile ahci_hba_mem_t* hba = (volatile ahci_hba_mem_t*)controller->abar;
    uint32_t pending = hba->is;
    if(pending == 0) {
      continue;
    }

    serviced = true;
    hba->is = pending;

    uint32_t implemented = hba->pi;
    uint32_t active_ports = pending & implemented;
    for(uint32_t port_index = 0; port_index < AHCI_MAX_PORTS; port_index++) {
      if(((active_ports >> port_index) & 0x1u) == 0) {
        continue;
      }

      ahci_port_t* port = controller->ports ? &controller->ports[port_index] : NULL;
      volatile ahci_hba_port_t* regs = &hba->ports[port_index];
      uint32_t port_pending = regs->is;
      if(port_pending != 0) {
        regs->is = port_pending;
      }

      if(port_pending & HBA_PxIS_TFES) {
        regs->serr = regs->serr;
      }

      if(port != NULL && port->registered && (port_pending != 0)) {
#if AHCI_VERBOSE_LOG
        serial_printf("ahci: irq ctrl %02x:%02x.%u port %u pending=0x%08x\n",
                      controller->bus,
                      controller->device,
                      controller->function,
                      port_index,
                      port_pending);
#endif
      }
    }
  }

  apic_send_eoi();

#if AHCI_VERBOSE_LOG
  if(!serviced) {
    serial_printf("ahci: spurious interrupt (no controller reported pending bits)\n");
  }
#endif
}

void ahci_init(void) {
  if(ahci_initialized) {
    return;
  }

  controllers_head = NULL;
  controllers_count = 0;

  ahci_scan_bus();

  serial_printf("ahci: discovered %zu controller(s)\n", controllers_count);
  ahci_initialized = true;
}

ahci_controller_t* ahci_controllers(void) {
  return controllers_head;
}

size_t ahci_controller_count(void) {
  return controllers_count;
}
