#pragma once

#include <stdbool.h>
#include <menios/input.h>

void keyboard_event_push(const menios_key_event_t* event);
bool keyboard_event_try_pop(menios_key_event_t* event);
