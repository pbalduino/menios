#pragma once

// PS/2 Controller Ports
#define PS2_DATA_PORT          0x60
#define PS2_COMMAND_PORT       0x64

// PS/2 Commands
#define PS2_ENABLE_FIRST_PORT  0xae
#define PS2_DISABLE_FIRST_PORT 0xad
#define PS2_WRITE_MODE         0x60
#define PS2_ENABLE_SCANNING    0xf4
#define PS2_RESET_COMMAND      0xff