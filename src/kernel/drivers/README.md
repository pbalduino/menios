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
