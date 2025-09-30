# Road to a Rich Graphic Interface on meniOS

🎯 **Goal**: Evolve meniOS from a debug-friendly framebuffer into a full
graphical environment capable of hosting windowed applications, composited
desktops, and hardware-accelerated rendering.

## Current Capabilities

- Limine boots into a linear framebuffer and passes mode information to the
  kernel.
- `SYS_FB_GETINFO`, `SYS_FB_MAP`, and `SYS_FB_FLIP` expose the buffer to userland
  with double-buffered staging (#31).
- Input stack delivers keyboard events via `/dev/input/kbd` (#32); mouse support
  is pending (#143/#144).
- `/dev/console` multiplexes framebuffer output with serial logging for terminal
  apps (#138).
- Shell now redirects STDIN/STDOUT to `/dev/console`, giving live VGA output (#166).
- Character device registry (#170 ✅) and VGA text driver (#168 ✅) expose `/dev/vga/0`, so console output no longer depends on the framebuffer helper alone.
- `/dev/zero` (#167) now comes from the same character-device infrastructure, so userland can rely on the
  conventional zero source.
- TTY subsystem now provides canonical terminal I/O and `/dev/tty0` (#169).
- PID 1 `init` supervisor is in place (#153/#154), enabling userland services to
  launch at boot and supervise graphical daemons.

## Vision

1. **Stable graphics subsystem** with predictable mode switches and palette
   control.
2. **Windowing/compositor layer** that multiplexes the framebuffer across
   multiple clients.
3. **User interface toolkit** providing font rendering, widgets, and layout.
4. **Hardware acceleration** via GPU drivers or software rasterization fallback.

## Roadmap Phases

### Phase 1: Framebuffer Polish (0-2 months)
- Add mode enumeration and dynamic switching (`SYS_FB_ENUM`, `SYS_FB_SETMODE`).
- Implement palette updates and gamma control for 8-bit modes.
- Provide a vsync/scanline interrupt hook for tear-free flips.
- Document buffer alignment, pitch, and pixel format guarantees.

### Phase 2: Input Completeness (parallel)
- Land PS/2 mouse driver (#143) and USB HID mouse support (#144/#125).
- Normalise pointer events in `/dev/input/mouse0` with relative and absolute modes.
- Introduce keyboard layout switching and modifier tracking.

### Phase 3: Surface Abstraction (2-4 months)
- Design a `/dev/fb/*` hierarchy via devfs (#152) exposing multiple logical
  surfaces.
- Add `SYS_FB_CREATE_SURFACE` / `SYS_FB_DESTROY` to allocate off-screen buffers.
- Support blit operations (copy, fill, alpha blend) in kernel or trusted
  userspace service.
- Integrate with shared memory (#104) for zero-copy surface updates.

### Phase 4: Compositor & Window Manager (4-6 months)
- Launch a userland compositor as a managed service under PID 1.
- Define a lightweight protocol (inspired by Wayland/SurfaceFlinger) for window
  creation, input focus, and damage tracking.
- Implement z-ordering, clipping, and cursor composition.
- Provide IPC hooks for client toolkits (named pipes or message queues).

### Phase 5: UI Toolkit & Applications (6-9 months)
- Bundle font rendering utilities (leverage Unicode roadmap #127-#134).
- Offer basic widgets: buttons, text boxes, menus.
- Create a terminal emulator using the compositor protocol.
- Port a demo app (file viewer, image viewer) to validate the stack.

### Phase 6: Acceleration & Advanced Features (9+ months)
- Investigate software rasterisers (e.g., TinyGL/Skia) for composition offload.
- Research GPU driver bring-up for common virtual devices (Bochs VBE, Virtio-GPU).
- Enable multi-monitor layouts once hardware probing allows.

## Key Dependencies

- **Devfs** (#152) and **procfs** (#153) for device discovery and telemetry.
- **Shared memory** (#104) and **futex/message IPC** (#105-#107) for efficient
  compositor-client communication.
- **Thread-safe libc** (#110) and **pthread API** (#109) to support
  multithreaded GUI libraries.
- **Unicode support** (#127-#134) for international text rendering.

## Testing & Tooling

- Extend existing userland demos to draw into off-screen surfaces and submit to
  the compositor.
- Add headless renderer stubs for CI (dump surfaces to PNG for regression
  testing).
- Leverage `com1.log` tracing toggles for timing diagnostics.

## Success Criteria

- Boot launches compositor and an interactive window manager under PID 1.
- Multiple graphical applications render concurrently without flicker.
- Input routing (keyboard, mouse) respects focus and delivers low latency.
- Shell, terminal emulator, and sample GUI apps operate entirely in userland.

With the init supervisor and upcoming shell plumbing in place (#151/#152), the
graphics roadmap can progress in parallel with the audio and multi-user efforts
documented elsewhere.
