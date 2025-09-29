# Hardware Probing & Driver Registry

Issue #22 tracks hardware discovery in meniOS. The registry now works as follows:

* `driver_init()` seeds the driver list (PS/2 keyboard, PCI root bridge, etc.), and `driver_register()` keeps a copy in kernel memory.
* Drivers that should be hot immediately (for example the legacy PS/2 keyboard) call their `start()` hook from `*_init()` so that early consoles have input even before ACPI enumeration reaches them.
* ACPI enumeration (`hardware_init() -> acpi_enumerate()`) walks the namespace, logging every device with a Hardware ID (HID) and calling `driver_load()`.
* `driver_load()` now returns the matched driver descriptor and invokes its `start()` hook. Devices whose interrupts are routed through the IOAPIC keep their matching PIC lines masked so the handler only fires once.
* `hardware_device_register()` records every detected device, storing its ACPI path, HID, and matched driver. `hardware_log_devices()` prints a summary for diagnostics at the end of hardware discovery.

This lays the groundwork for plugging in PCI and other bus enumerators while keeping all detected devices visible from the kernel side.

_Last updated: 2025-09-27_
