#ifndef MENIOS_INCLUDE_KERNEL_WORKQUEUE_H
#define MENIOS_INCLUDE_KERNEL_WORKQUEUE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WORKQUEUE_CLASS_GENERIC = 0,
  WORKQUEUE_CLASS_ACPI_GPE,
  WORKQUEUE_CLASS_ACPI_NOTIFY
} workqueue_class_t;

typedef void (*workqueue_handler_t)(void *ctx);

void workqueue_initialize(void);
void workqueue_start(void);
int workqueue_submit(workqueue_class_t cls, workqueue_handler_t handler, void *ctx);
void workqueue_wait_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_KERNEL_WORKQUEUE_H */
