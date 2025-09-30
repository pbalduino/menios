# Sound System Architecture in meniOS

meniOS does not yet ship a finished audio stack, but the surrounding kernel
infrastructure (timers, process supervision, VFS, and the syscall layer) is now
strong enough to host one. This document lays out the architectural plan for
capturing, mixing, and presenting sound in the operating system.

## Design Goals

- **Low-latency playback**: Keep round-trip latency under a few milliseconds so
  games and interactive apps remain responsive.
- **Deterministic timing**: Build on the calibrated LAPIC/HPET infrastructure
  (#101) to schedule buffer flips predictably.
- **Userspace friendliness**: Expose the audio path through character devices
  and syscalls that match the style used by framebuffer/input.
- **Mixing & format conversion**: Provide a kernel mixer that can blend multiple
  PCM streams while performing simple resampling when needed.
- **Reliability**: Recover gracefully from underruns/overruns and isolate buggy
  clients so they cannot wedge the kernel.

## Hardware Expectations

Initial work targets AC'97/HDA-style PCI audio controllers exposed via the
existing PCI probe path. Future work can add USB or legacy ISA back-ends, but
the architecture assumes:

- DMA-capable hardware with separate playback/capture channels
- Interrupt delivery for buffer position updates
- Programmable sample format (8/16-bit PCM) and sampling rate (11–48 kHz)

## Kernel Components

### Driver Layer

1. **PCI audio driver** (#33)
   - Enumerates capable devices during `hardware_init()`
   - Sets up memory-mapped I/O regions and BARs
   - Configures DMA descriptors for playback/capture rings
   - Hooks into the existing interrupt dispatcher

2. **DMA Engine**
   - Allocates physically contiguous buffers via `dma_alloc`
   - Maintains twin (ping/pong) or triple buffering for smooth playback
   - Exposes buffer state to higher layers

3. **Interrupt Handler**
   - Wakes the mixer thread when the hardware finishes a period
   - Updates monotonic playback position counters
   - Detects underruns/overruns and raises events for userland diagnostics

### Mixer & Scheduler

- Runs as a kernel thread pinned to `PROC_PRIO_REALTIME`
- Pulls PCM frames from registered clients (user processes or in-kernel sound
  services)
- Applies gain, simple resampling, and channel mixing
- Commits the mixed buffer to the DMA ring and arms the next interrupt
- Uses LAPIC timer alarms to pre-fill buffers if interrupts arrive late

### Control Path

- ioctls or dedicated syscalls configure sample rate, channel count, and buffer
  size
- Mixer maintains per-client metadata (volume, state, format)
- Provides statistics via `/proc/sound/*` once procfs exists (#147)

## Userspace Interface

### Device File Layout

- `/dev/dsp0` – primary playback device (read/write PCM)
- `/dev/mix0` – control interface for mixer settings
- `/dev/audio*` – compatibility aliases for simple streaming clients

Devices are registered via devfs (#146) once that infrastructure lands.

### Syscalls

- `sys_audio_config` – negotiate format (rate, channels, depth)
- `sys_audio_queue` – enqueue a buffer for playback (copy or shared memory)
- `sys_audio_poll` – wait for buffer completion with timeout support
- `sys_audio_capture` – optional path for recording

The initial implementation may expose these operations via `ioctl` on the
device nodes, but distinct syscalls keep the door open for L4/L4Re-style
userland servers later.

### Buffering Strategy

- Default to double buffering (two periods per stream)
- Allow clients to request more periods if latency is less critical
- Use shared memory (`shmget`/`mmap`) once #104 is complete; otherwise fall back
  to kernel copies

## Synchronisation & Timing

- **Timers**: rely on LAPIC/HPET interrupts (#101) to keep the mixer on schedule
- **Locks**: use kernel mutexes for the client registry and per-stream state
- **Priority**: mixer thread inherits real-time priority to avoid starvation
- **Underrun Handling**: insert silence and notify clients; repeated underruns
  can trigger automatic stream throttling

## Diagnostics & Testing

- Logging via `serial_printf` for buffer events during bring-up
- `/proc/sound/status` for FIFO depth, underruns, and timestamping (post-procfs)
- Loopback mode (playback → capture) to validate end-to-end latency
- Unity tests for resampler/mixer math in `test/`

## Roadmap & Dependencies

1. Implement core audio driver and mixer skeleton (#33)
2. Land shared memory primitives (#104) for zero-copy queueing
3. Deliver devfs (#146) to publish device nodes
4. Add procfs (#147) for runtime observability
5. Expose higher-level APIs (ALSA-like library) once libc is thread-safe (#110)

With the PID 1 init process now supervising services and waitpid/zombie plumbing
in place (#149/#150/#154), audio daemons can run as dedicated user processes in
future microkernel-inspired builds.
