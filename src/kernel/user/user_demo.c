#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <kernel/heap.h>
#include <kernel/proc.h>
#include <kernel/serial.h>

extern const uint8_t user_demo_elf_start[];
extern const uint8_t user_demo_elf_end[];

typedef struct user_demo_spec_t {
  const char* name;
  uint8_t     priority;
} user_demo_spec_t;

static const user_demo_spec_t demo_specs[] = {
  { "user_demo_low",     PROC_PRIO_LOW },
  { "user_demo_normal",  PROC_PRIO_NORMAL },
  { "user_demo_high",    PROC_PRIO_HIGH }
};

#define USER_DEMO_COUNT (sizeof(demo_specs) / sizeof(demo_specs[0]))

void user_demo_launch(void) {
  size_t code_size = (size_t)(user_demo_elf_end - user_demo_elf_start);
  if(code_size == 0) {
    serial_printf("user_demo_launch: stub size is zero\n");
    return;
  }

  serial_printf("user_demo_launch: scheduling %lu user demo processes\n",
                (unsigned long)USER_DEMO_COUNT);

  for(size_t i = 0; i < USER_DEMO_COUNT; i++) {
    proc_info_p proc = kmalloc(sizeof(proc_info_t));
    if(proc == NULL) {
      serial_printf("user_demo_launch: failed to allocate proc_info (%zu)\n", i);
      return;
    }
    memset(proc, 0, sizeof(proc_info_t));

    proc_create_user(proc,
                     demo_specs[i].name,
                     user_demo_elf_start,
                     code_size,
                     (void*)(uintptr_t)i);

    proc_set_priority(proc, demo_specs[i].priority);
    serial_printf("user_demo_launch: queued %s with priority %u\n",
                  demo_specs[i].name,
                  demo_specs[i].priority);

    proc_execute(proc);
  }
}
