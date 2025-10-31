#ifndef MENIOS_INCLUDE_KERNEL_IRQ_H
#define MENIOS_INCLUDE_KERNEL_IRQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IRQ_VECTOR_BASE  0x60
#define IRQ_VECTOR_LIMIT 0xef
#define IRQ_VECTOR_COUNT ((IRQ_VECTOR_LIMIT - IRQ_VECTOR_BASE) + 1)

struct irq_handle;

typedef bool (*irq_handler_fn)(void *ctx);

void irq_initialize(void);
void irq_apic_online(void);

int irq_register(uint32_t irq, irq_handler_fn handler, void *ctx, struct irq_handle **out_handle);
int irq_unregister(struct irq_handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_KERNEL_IRQ_H */
