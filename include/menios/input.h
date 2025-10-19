#ifndef MENIOS_INCLUDE_INPUT_H
#define MENIOS_INCLUDE_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MENIOS_KEY_MOD_SHIFT = 1u << 0,
  MENIOS_KEY_MOD_CTRL  = 1u << 1,
  MENIOS_KEY_MOD_ALT   = 1u << 2,
  MENIOS_KEY_MOD_CAPS  = 1u << 3,
} menios_key_modifier_t;

typedef struct menios_key_event {
  uint8_t scancode;   /* Set 1 scancode without release bit */
  uint8_t ascii;      /* Translated ASCII character (0 if none) */
  uint8_t pressed;    /* 1 when key pressed, 0 when released */
  uint8_t extended;   /* 1 if this was an extended (0xE0) scancode */
  uint8_t modifiers;  /* Combination of menios_key_modifier_t */
  uint8_t reserved[3];
} menios_key_event_t;

int menios_input_poll(menios_key_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_INPUT_H */
