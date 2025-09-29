#ifndef MENIOS_INCLUDE_KERNEL_INPUT_KEYBOARD_H
#define MENIOS_INCLUDE_KERNEL_INPUT_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


typedef struct file file_t;

#define KBD_MOD_LEFT_SHIFT   (1u << 0)
#define KBD_MOD_RIGHT_SHIFT  (1u << 1)
#define KBD_MOD_CTRL         (1u << 2)
#define KBD_MOD_ALT          (1u << 3)
#define KBD_MOD_CAPS_LOCK    (1u << 4)

typedef struct keyboard_event_t {
  uint64_t timestamp_ns;
  uint16_t scancode;
  uint8_t  ascii;
  uint8_t  modifiers;
  uint8_t  pressed;   /* 1 when the key is pressed, 0 on release */
  uint8_t  reserved[3];
} keyboard_event_t;

void keyboard_device_init(void);
void keyboard_device_reset(void);
void keyboard_device_enqueue(const keyboard_event_t* event);
file_t* keyboard_device_open(void);

#ifdef __cplusplus
}
#endif

#endif
