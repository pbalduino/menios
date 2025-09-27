# TSC Calibration & Boot Time

Issue #12 tracks accurate timekeeping across the kernel. The current implementation (2025‑09‑26) performs CPUID-based calibration, exposes conversion helpers, and updates the busy-wait sleeper to use calibrated values.

## Implementation Snapshot

* `tsc_calibrate()` reads CPUID leaves 0x15/0x16 to determine the TSC frequency (falling back to 1 GHz with a log message when unavailable).
* Helper APIs `tsc_ticks_to_ns()`, `tsc_ns_to_ticks()`, and `tsc_frequency_hz()` expose the calibration result; `tsc_override_calibration()` is available for unit tests.
* `ksleep()` now converts milliseconds to ticks via the helpers rather than assuming a 1 MHz TSC, so sleeps remain accurate after calibration.

## Next Steps

1. Add a fallback path using LAPIC/HPET/PIT if CPUID data is missing or inaccurate.
2. Integrate calibrated timing with future timer interrupts (instead of pure busy-waiting).
3. Surface calibration details through a diagnostic command or kernel log entry during boot.

This foundation ensures that subsequent scheduling and profiling work has a reliable notion of time.

_Last updated: 2025-09-26_
