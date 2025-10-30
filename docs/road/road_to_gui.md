# Road to GUI - Graphical User Interface Stack

**Status:** Planning
**Target:** Full graphical desktop environment with compositor and window manager
**Timeline:** 6-9 months (26-39 weeks)

---

## Vision

Build a complete graphical user interface stack for meniOS, enabling rich graphical applications, window management, and a modern desktop environment. The architecture follows a Wayland-style compositor model where the display server and compositor are unified.

### What We'll Be Able to Do

With a complete GUI stack, meniOS will support:

#### Core Graphics Capabilities
- **Vector Graphics**: High-quality 2D rendering with anti-aliasing
- **Text Rendering**: Beautiful font rendering with subpixel anti-aliasing
- **Image Compositing**: Alpha blending, transformations, gradients
- **PDF/SVG Output**: Generate documents and vector graphics
- **Hardware Acceleration**: Future GPU-accelerated rendering

#### Window Management
- **Multiple Windows**: Run many applications simultaneously
- **Window Decorations**: Title bars, borders, resize handles
- **Window Operations**: Move, resize, minimize, maximize, close
- **Focus Management**: Click-to-focus, focus-follows-mouse
- **Virtual Desktops**: Multiple workspaces (future)
- **Tiling Support**: Automatic window arrangement (future)

#### Application Types
- **Terminal Emulator**: Graphical terminal with Unicode support
- **Text Editor**: GUI text editor with syntax highlighting
- **File Manager**: Visual file browser with icons
- **Image Viewer**: Display PNG, JPEG, BMP images
- **Web Browser**: Simple HTML renderer (future)
- **Development Tools**: GUI debugger, hex editor, profiler
- **Games**: 2D games, puzzle games, arcade classics
- **System Monitor**: CPU, memory, process viewer
- **Settings Panel**: System configuration GUI

#### Desktop Environment Features
- **Desktop Background**: Wallpapers and color themes
- **Panel/Taskbar**: Application launcher, system tray, clock
- **Menus**: Context menus, application menus
- **Notifications**: System notifications and alerts
- **Drag and Drop**: File and window dragging
- **Clipboard**: Copy/paste between applications
- **Accessibility**: Screen reader support, high contrast themes (future)

---

## Architecture Overview

### Wayland-Style Compositor Model

```
┌─────────────────────────────────────────────────────────────┐
│                     GUI Applications                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ Terminal │  │  Editor  │  │  Browser │  │   Game   │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │              │             │          │
│       └─────────────┴──────────────┴─────────────┘          │
│                          │                                   │
│                   Client Protocol                            │
│                     (IPC/Sockets)                            │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│              MeniOS Compositor (Display Server)              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │            Window Manager (Integrated)                  │ │
│  │  - Window placement, sizing, stacking                   │ │
│  │  - Focus management, decorations                        │ │
│  │  - Input event routing                                  │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Scene Graph & Compositing                  │ │
│  │  - Damage tracking, dirty rectangles                    │ │
│  │  - Double buffering, vsync                              │ │
│  │  - Window textures, transformations                     │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │                  Input Management                       │ │
│  │  - Keyboard events → focused window                     │ │
│  │  - Mouse events → window hit testing                    │ │
│  │  - Touch events (future)                                │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Graphics Stack (Cairo)                      │
│  ┌────────────────────────────────────────────────────────┐ │
│  │    Cairo 2D Graphics Library (Vector Rendering)         │ │
│  │  - Paths, fills, strokes                                │ │
│  │  - Text rendering with FreeType                         │ │
│  │  - Image compositing, transformations                   │ │
│  └───────────────────┬────────────────────────────────────┘ │
│  ┌──────────────────▼─────────────────────────────────────┐ │
│  │         Pixman (Low-Level Pixel Manipulation)           │ │
│  │  - Software rasterization                               │ │
│  │  - Compositing operations                               │ │
│  │  - SIMD optimizations (SSE2/AVX)                        │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Kernel Graphics Support                     │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Framebuffer Driver (/dev/fb0)              │ │
│  │  - Linear framebuffer access                            │ │
│  │  - Mode setting (resolution, bpp)                       │ │
│  │  - Page flipping (vsync)                                │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │         Input Drivers (Keyboard, Mouse, Touch)          │ │
│  │  - PS/2 keyboard (#37)                                  │ │
│  │  - PS/2 mouse (#143)                                    │ │
│  │  - USB HID (future)                                     │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │         Shared Memory & IPC (Client Communication)      │ │
│  │  - Shared framebuffers                                  │ │
│  │  - Unix domain sockets                                  │ │
│  │  - Event queues                                         │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

1. **Wayland-Style Architecture**: Compositor is the display server (not X11)
   - Simpler, more secure
   - Better performance (direct rendering)
   - Modern approach used by most Linux desktops

2. **Cairo for 2D Graphics**: Industry-standard, mature library
   - Used by GTK, Firefox, Inkscape, LibreOffice
   - Excellent text rendering with FreeType
   - PDF/SVG support built-in

3. **Integrated Window Manager**: Part of compositor, not separate
   - Tighter integration, better performance
   - Easier to maintain consistency
   - Can still support pluggable policies

4. **Shared Memory for Performance**: Zero-copy window buffers
   - Clients render to shared memory
   - Compositor composites without copying
   - Critical for video and games

---

## Milestone Breakdown

### Milestone 1: Graphics Foundation (8-10 weeks)

**Goal:** Get Cairo rendering to the framebuffer

#### Issues
- **#397: Port Pixman** (3-4 weeks)
  - Low-level pixel manipulation library
  - Cairo dependency
  - SIMD optimizations for x86-64

- **#398: Port FreeType** (2-3 weeks)
  - TrueType/OpenType font rendering
  - Required for Cairo text support

- **#396: Port Cairo** (3-4 weeks)
  - Core 2D graphics library
  - Framebuffer backend
  - Image and PDF surfaces

#### Deliverables
- [ ] Cairo library compiled and running on meniOS
- [ ] Can render basic shapes to framebuffer
- [ ] Can render text with TrueType fonts
- [ ] Example programs: hello world, clock, graphics demo

#### Example Code
```c
// Simple Cairo demo
#include <cairo/cairo.h>

int main() {
    // Create Cairo surface for framebuffer
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, 1024, 768);
    cairo_t *cr = cairo_create(surface);

    // Draw a blue rectangle
    cairo_set_source_rgb(cr, 0.2, 0.3, 0.8);
    cairo_rectangle(cr, 100, 100, 400, 300);
    cairo_fill(cr);

    // Draw text
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_select_font_face(cr, "Sans",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 48);
    cairo_move_to(cr, 150, 250);
    cairo_show_text(cr, "Hello, meniOS!");

    // Blit to framebuffer
    int fb = open("/dev/fb0", O_RDWR);
    write(fb, cairo_image_surface_get_data(surface), 1024*768*4);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
```

---

### Milestone 2: Input Stack (4-5 weeks)

**Goal:** Unified input event system for GUI

#### Issues
- **#399: Input Event System** (2-3 weeks)
  - Abstract input events (keyboard, mouse, touch)
  - Event queue and dispatch
  - Integration with existing PS/2 drivers

- **#400: Mouse Cursor Rendering** (1-2 weeks)
  - Hardware cursor or software cursor
  - Cursor themes and animations
  - Hit testing for window selection

#### Deliverables
- [ ] Unified input event API
- [ ] Mouse cursor visible on screen
- [ ] Can click and receive events
- [ ] Example: Interactive drawing program

#### Dependencies
- Blocks on: #143 (PS/2 Mouse Driver)
- Uses: #37 (PS/2 Keyboard - already complete)

---

### Milestone 3: Shared Memory & IPC (3-4 weeks)

**Goal:** Client-server communication infrastructure

#### Issues
- **#401: Shared Memory Implementation** (2-3 weeks)
  - POSIX shared memory (shm_open, mmap)
  - Shared framebuffers for windows
  - Synchronization primitives

- **#402: Unix Domain Sockets** (1-2 weeks)
  - AF_UNIX socket support
  - Compositor protocol transport
  - Credential passing (SCM_RIGHTS)

#### Deliverables
- [ ] Clients can allocate shared memory
- [ ] Compositor can map client buffers
- [ ] Socket communication working
- [ ] Example: Client renders, compositor displays

#### Example Usage
```c
// Client allocates window buffer
int fd = shm_open("/window_123", O_CREAT|O_RDWR, 0600);
ftruncate(fd, 1024 * 768 * 4);
void *pixels = mmap(NULL, 1024*768*4, PROT_READ|PROT_WRITE,
                     MAP_SHARED, fd, 0);

// Client renders to shared memory
cairo_surface_t *surface =
    cairo_image_surface_create_for_data(pixels,
        CAIRO_FORMAT_ARGB32, 1024, 768, 1024*4);
// ... render with Cairo ...

// Send buffer to compositor via socket
compositor_attach_buffer(fd, 1024, 768);
```

---

### Milestone 4: Compositor Core (6-8 weeks)

**Goal:** Basic compositor that can display windows

#### Issues
- **#403: Compositor Core** (3-4 weeks)
  - Window surface management
  - Scene graph and rendering pipeline
  - Damage tracking

- **#404: Window Protocol** (2-3 weeks)
  - Define client-compositor protocol
  - Surface creation, buffer attachment
  - Frame callbacks, damage reporting

- **#405: Composition Engine** (2-3 weeks)
  - Render all windows to screen
  - Z-order management
  - Double buffering, vsync
  - Dirty rectangle optimization

#### Deliverables
- [ ] Compositor runs as userland daemon
- [ ] Can create and display windows
- [ ] Multiple windows composited correctly
- [ ] Smooth rendering, no tearing
- [ ] Example: Run multiple Cairo apps

#### Architecture
```c
// Compositor state
struct compositor {
    cairo_surface_t *screen;  // Framebuffer surface
    struct window *windows;    // Linked list
    struct window *focused;    // Current focus
    int damage_dirty;          // Needs repaint
};

struct window {
    uint32_t id;
    int x, y, width, height;
    int mapped, visible;
    cairo_surface_t *surface;  // Shared memory surface
    struct window *next;
};

// Render loop
void compositor_render(struct compositor *comp) {
    if (!comp->damage_dirty) return;

    // Clear screen
    cairo_t *cr = cairo_create(comp->screen);
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    // Composite windows back-to-front
    for (struct window *w = comp->windows; w; w = w->next) {
        if (!w->visible) continue;

        cairo_set_source_surface(cr, w->surface, w->x, w->y);
        cairo_rectangle(cr, w->x, w->y, w->width, w->height);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    comp->damage_dirty = 0;
}
```

---

### Milestone 5: Window Manager (5-7 weeks)

**Goal:** Full window management capabilities

#### Issues
- **#406: Window Manager Core** (3-4 weeks)
  - Window placement policies
  - Window decorations (title bar, borders)
  - Move, resize, minimize, maximize, close

- **#407: Focus Management** (1-2 weeks)
  - Click-to-focus
  - Focus-follows-mouse (optional)
  - Keyboard focus and tab cycling

- **#408: Window Decorations** (2-3 weeks)
  - Title bar rendering
  - Resize handles
  - Minimize/maximize/close buttons
  - Active/inactive styling

#### Deliverables
- [ ] Windows have decorations
- [ ] Can move windows by dragging title bar
- [ ] Can resize windows by dragging edges
- [ ] Can close windows via close button
- [ ] Keyboard focus works correctly
- [ ] Example: Multiple interactive apps

#### User Experience
```
┌─────────────── Terminal ──────────────────[_][□][X]─┐
│ $ cat /etc/motd                                      │
│ Welcome to meniOS!                                   │
│ $                                                    │
│                                                      │
└──────────────────────────────────────────────────────┘

┌─────────────── Text Editor ───────────────[_][□][X]─┐
│ File  Edit  View  Help                              │
│ ─────────────────────────────────────────────────── │
│ /* Hello, World! */                                  │
│ int main() {                                         │
│     printf("Hello, meniOS!\n");                      │
│     return 0;                                        │
│ }                                                    │
└──────────────────────────────────────────────────────┘
```

---

### Milestone 6: Desktop Environment (6-8 weeks)

**Goal:** Complete desktop with panel, menus, applications

#### Issues
- **#409: Desktop Shell** (3-4 weeks)
  - Desktop background
  - Panel/taskbar
  - Application launcher
  - System tray

- **#410: Core GUI Applications** (3-4 weeks)
  - Terminal emulator
  - Text editor
  - File manager
  - Image viewer
  - Calculator

#### Deliverables
- [ ] Desktop with panel and background
- [ ] Can launch applications from menu
- [ ] System tray with clock
- [ ] At least 3 GUI applications working
- [ ] Cohesive look and feel

#### Desktop Layout
```
┌────────────────────────────────────────────────────────────┐
│  Applications  System  Help                          🔊 🔋 │ ← Panel
├────────────────────────────────────────────────────────────┤
│                                                            │
│                      [Desktop Background]                  │
│                                                            │
│   ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                │
│   │ Term │  │ Edit │  │ Files│  │ Calc │  ← App icons    │
│   └──────┘  └──────┘  └──────┘  └──────┘                 │
│                                                            │
│                                                            │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-10)
- Port Pixman, FreeType, Cairo
- Get basic rendering working
- **Milestone:** "Hello, World!" with Cairo

### Phase 2: Input & IPC (Weeks 11-18)
- Input event system
- Shared memory
- Unix sockets
- **Milestone:** Interactive Cairo demo

### Phase 3: Compositor (Weeks 19-26)
- Compositor core
- Window protocol
- Composition engine
- **Milestone:** Multiple windows displaying

### Phase 4: Window Management (Weeks 27-33)
- Window manager
- Decorations
- Focus management
- **Milestone:** Full window operations

### Phase 5: Desktop (Weeks 34-41)
- Desktop shell
- Panel and menus
- Core applications
- **Milestone:** Complete desktop environment

### Phase 6: Polish & Extensions (Weeks 42+)
- Performance optimization
- Additional applications
- Accessibility features
- **Future:** 3D support (OpenGL/Vulkan)

---

## Dependencies

### Kernel Features Required
- ✅ Framebuffer driver (`/dev/fb0`)
- ✅ Keyboard driver (#37 - PS/2 keyboard)
- ⏳ Mouse driver (#143 - PS/2 mouse) - **CRITICAL BLOCKER**
- ⏳ Shared memory (new)
- ⏳ Unix domain sockets (new)
- ✅ Process management (fork/exec)
- ✅ Signals
- ⏳ Robust IPC

### Userland Features Required
- ✅ libc (#193 - mostly complete)
- ✅ Dynamic memory (malloc/free)
- ⏳ Threading (#109 - pthread API)
- ⏳ Thread-safe libc (#339)
- ✅ File I/O
- ⏳ Socket API (#341 - future)

### External Libraries to Port
1. **Pixman** (0.42.x)
   - ~50,000 lines of C
   - SIMD optimizations (SSE2, SSSE3, AVX2)
   - No external dependencies

2. **FreeType** (2.13.x)
   - ~100,000 lines of C
   - Minimal dependencies
   - Optional: libpng for PNG fonts

3. **Cairo** (1.18.x)
   - ~150,000 lines of C
   - Depends on: Pixman, FreeType
   - Multiple backends (image, PDF, SVG)

---

## Timeline Summary

| Milestone | Duration | Cumulative |
|-----------|----------|------------|
| 1. Graphics Foundation | 8-10 weeks | 10 weeks |
| 2. Input Stack | 4-5 weeks | 15 weeks |
| 3. Shared Memory & IPC | 3-4 weeks | 19 weeks |
| 4. Compositor Core | 6-8 weeks | 27 weeks |
| 5. Window Manager | 5-7 weeks | 34 weeks |
| 6. Desktop Environment | 6-8 weeks | 42 weeks |

**Total:** 32-42 weeks (8-10 months)

---

## Success Criteria

### Minimal GUI (6 months)
- [ ] Cairo rendering to framebuffer
- [ ] Mouse and keyboard input working
- [ ] Compositor displaying multiple windows
- [ ] Basic window management (move, resize, close)
- [ ] At least 1 GUI application (terminal emulator)

### Full Desktop (9 months)
- [ ] Desktop shell with panel
- [ ] Application launcher and menus
- [ ] Window decorations and full WM features
- [ ] 3+ GUI applications (terminal, editor, file manager)
- [ ] Smooth performance, no visible tearing
- [ ] Professional look and feel

### Future Extensions
- [ ] Widget toolkit for easy app development
- [ ] More applications (browser, media player, games)
- [ ] Accessibility features
- [ ] OpenGL/Vulkan 3D support
- [ ] Multi-monitor support
- [ ] Network transparency (remote apps)

---

## Related Issues

### Blockers
- #143 - PS/2 Mouse Driver (CRITICAL - needed for GUI input)
- #109 - pthread API (needed for compositor threading)
- #339 - Thread-safe libc (needed for multi-threaded apps)

### Related
- #129 - Unicode text rendering (complements Cairo text)
- #394 - Terminal scrollback (will use in GUI terminal)
- #170 - Character device infrastructure (input devices)

### Enables
- Advanced text editor
- Web browser
- Image editor
- Video player
- Games with rich graphics
- Development tools (GUI debugger, profiler)

---

## References

### Technical Specifications
- **Wayland Protocol**: https://wayland.freedesktop.org/docs/html/
- **Cairo Documentation**: https://www.cairographics.org/manual/
- **Pixman**: https://www.pixman.org/
- **FreeType**: https://freetype.org/

### Inspiration
- **Weston** (Reference Wayland compositor)
- **Sway** (Tiling Wayland compositor)
- **Arcan** (Novel display server architecture)
- **SerenityOS** (Hobby OS with complete GUI)
- **ToaruOS** (Compositor and window manager reference)

### Example Compositor Projects
- **tinywl** - Minimal Wayland compositor (~500 lines)
- **wlroots** - Compositor library used by Sway
- **Smithay** - Rust Wayland compositor library

---

## Notes

- **Why not X11?** Too complex, legacy design, security issues
- **Why Cairo?** Industry standard, excellent quality, well documented
- **Why Wayland-style?** Modern, secure, better performance
- **GPU acceleration?** Future work after software rendering proven

This roadmap represents a significant but achievable goal for meniOS. Each milestone builds incrementally, with working deliverables at every stage.
