# TSC Calibration & Boot Time Initialisation Plan

Issue #12 covers accurate timekeeping: calibrating the Time Stamp Counter (TSC) and populating the boot-time value exposed to both kernel subsystems and userland diagnostics.

## Current Behaviour

* `src/kernel/tsc.c` (not shown here) performs a basic initialisation but assumes the TSC frequency derived from hardware defaults; drift or turbo modes are not accounted for.
* Boot time (`boot_time()` in `src/kernel/timer/timer.c`) relies on Limine’s boot-time request. If unavailable it halts, but no calibration is performed to align TSC ticks with wall-clock time.
* `ksleep` in `kthread.c` simply busy-waits comparing raw `read_tsc()` values to a target with no scaling, so it only works if the TSC increments at 1 MHz.

## Goals

1. Determine the TSC frequency at boot, falling back to LAPIC timer/HPET calibration if necessary.
2. Convert TSC ticks to nanoseconds consistently across the kernel (e.g., via `tsc_ticks_to_ns()` helper).
3. Populate the boot-time structure using calibrated values so `logk`/`errk` output shows accurate timestamps.
4. Provide an API for sleeping based on calibrated timings (e.g., replace busy-wait with calibrated spin + eventual integration with timer interrupts).

## Implementation Steps

1. **Calibration Phase**
   * Use known-frequency hardware (PIT, HPET, LAPIC timer) to measure TSC increments over a fixed interval.
   * Store the calculated frequency in a global (`tsc_khz` or similar).

2. **Helpers**
   * Introduce inline conversion helpers `tsc_ticks_to_ns()` and `ns_to_tsc_ticks()`.
   * Update `ksleep` and other timing code to use these helpers instead of raw multipliers.

3. **Boot Time**
   * During early boot, set a monotonic start timestamp combining Limine’s boot time (seconds) with calibrated TSC for sub-second precision.
   * Update logging utilities to use calibrated conversions.

4. **Diagnostics**
   * Add a debug log summarising the measured TSC frequency and calibration source (HPET/LAPIC/etc.).
   * Expose a simple `tsc_info` command or kernel log entry for troubleshooting.

With calibration in place, subsequent timing-dependent work (e.g., SMP scheduler ticks, timers) will have a reliable baseline.

_Last updated: 2025-09-26_
