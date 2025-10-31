# Hardware Probing & Driver Registry

Issue #22 tracks hardware discovery in meniOS. The registry now works as follows:

* `driver_registry_init()` seeds the driver list (PS/2 keyboard, PCI root bridge, etc.), and `driver_register()` keeps a copy in kernel memory.
* ACPI enumeration (`hardware_initialize() -> acpi_enumerate()`) walks the namespace, logging every device with a Hardware ID (HID) and calling `driver_load()`.
* `driver_load()` now returns the matched driver descriptor and invokes its `start()` hook.
* `hardware_device_register()` records every detected device, storing its ACPI path, HID, and matched driver. `hardware_log_devices()` prints a summary for diagnostics at the end of hardware discovery.

This lays the groundwork for plugging in PCI and other bus enumerators while keeping all detected devices visible from the kernel side.

## ACPI Runtime Integration

Runtime services are handled through uACPI. The glue layer in
`src/kernel/acpi/uacpi_menios.c` now relies on the kernel's multitasking
primitives so AML methods can block without stalling the system:

- Events use `ksem_t`, allowing `Signal()/Wait()` pairs to coordinate work across
  threads and interrupt contexts.
- `Sleep()` delegates to `ksleep()`, so AML delays respect the scheduler.
- Tick queries call `ns_from_boot()` and return 100 ns units, providing the
  monotonic clock required for AML timeouts.

Deferred work scheduling still needs to be wired into a dedicated kernel thread,
and PCI/IO helpers remain stubs, but the new primitives unblock AML handlers
that previously failed at runtime.

_Last updated: 2025-10-15_
