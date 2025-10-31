# meniOS Kernel Drivers

This directory groups device drivers by subsystem so it is easier to find and
extend hardware support. Each subfolder maps to the structure outlined in
issue #23 (driver refactor phase):

- `audio/` – sound and mixer drivers (e.g., Sound Blaster stubs).
- `block/` – block-storage controllers such as AHCI/SATA.
- `bus/` – bus discovery and enumeration logic (PCI, ACPI hooks, …).
- `core/` – driver registry and shared infrastructure helpers.
- `input/` – human-interface devices like PS/2 keyboards or mice.

When introducing a new driver, place it in the appropriate subsystem directory
and add a matching header under `include/kernel/drivers/<subsystem>/`.

## Character Device Registration Quick Notes

- Use `char_device_register()` from `include/kernel/fs/devfs/devfs.h` to expose
  new `/dev/*` nodes. The helper allocates `dev_t` numbers on demand and wires
  the device into `devfs`.
- Dynamic majors now start at **10**; values below that range are reserved for
  the built-in pseudo devices (mem, tty, console, etc.). Drivers that relied on
  low-numbered majors should request an explicit `dev` if they need a legacy
  assignment.
- If a driver needs to hold on to a static major in advance (e.g., multi-device
  families), call `char_device_reserve_major()` before registering to keep the
  allocator from reusing the slot.
