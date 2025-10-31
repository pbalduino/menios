# GUI Architecture - Graphical User Interface Stack

## Goals

- Provide a complete graphical user interface stack for meniOS with modern desktop capabilities
- Implement a Wayland-style compositor architecture where the display server and compositor are unified
- Enable rich graphical applications through Cairo 2D graphics with vector rendering and high-quality text
- Support multiple windows with full window management (move, resize, minimize, maximize, close)
- Deliver smooth 60fps rendering with efficient damage tracking and double buffering
- Create a foundation extensible to future 3D graphics (OpenGL/Vulkan) and accessibility features

## Design Overview

The GUI stack follows a **Wayland-style compositor model** rather than the legacy X11 client-server architecture. This design unifies the display server and compositor into a single component, eliminating unnecessary round-trips and improving both security and performance.

### Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                     GUI Applications                         │
│           (Terminal, Editor, File Manager, etc.)             │
└──────────────────────────┬──────────────────────────────────┘
                           │ Window Protocol (IPC)
┌──────────────────────────▼──────────────────────────────────┐
│              MeniOS Compositor (Display Server)              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │            Window Manager (Integrated)                  │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │          Scene Graph & Composition Engine               │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Input Event Dispatcher                     │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Cairo 2D Graphics Library                   │
│              (Pixman + FreeType backends)                    │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Kernel Graphics & Input                     │
│  - Framebuffer (/dev/fb0)  - PS/2 Keyboard (#37)            │
│  - Shared Memory           - PS/2 Mouse (#143)              │
│  - Unix Sockets            - Event Queues                   │
└─────────────────────────────────────────────────────────────┘
```

### Why Wayland-Style Over X11?

**X11 Problems:**
- Client-server separation adds latency (every draw requires server round-trip)
- Complex protocol accumulated over 30+ years (millions of lines of code)
- Security issues (any client can read keystrokes from any window)
- Difficult to implement features like tear-free rendering

**Wayland-Style Benefits:**
- **Direct rendering**: Clients render to shared memory buffers, compositor just composites
- **Simpler protocol**: Only what's needed for modern graphics (a few thousand lines)
- **Better security**: Input isolation (only focused window receives keyboard events)
- **Modern features**: Easier to implement vsync, damage tracking, multi-monitor

### Key Components

#### 1. Cairo Graphics Library (#396)
**Purpose:** Industry-standard 2D vector graphics rendering

**Capabilities:**
- Vector paths with anti-aliasing (rectangles, circles, Bezier curves)
- Gradients (linear, radial) and pattern fills
- Image compositing with alpha blending
- Transformation matrices (translate, rotate, scale)
- PDF and SVG output surfaces
- Text rendering via FreeType integration

**Integration:**
- Renders to image surfaces backed by shared memory or framebuffer
- Used by both compositor (for window decorations, desktop shell) and applications
- Located in `vendor/cairo-1.18.0/` with meniOS-specific framebuffer backend

**Example:**
```c
cairo_surface_t *surface = cairo_fb_surface_create("/dev/fb0");
cairo_t *cr = cairo_create(surface);

// Vector rectangle with gradient
cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, 200, 200);
cairo_pattern_add_color_stop_rgb(grad, 0, 1, 0, 0);  // Red
cairo_pattern_add_color_stop_rgb(grad, 1, 0, 0, 1);  // Blue
cairo_rectangle(cr, 100, 100, 400, 300);
cairo_set_source(cr, grad);
cairo_fill(cr);

// Anti-aliased text
cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
cairo_set_font_size(cr, 48);
cairo_set_source_rgb(cr, 1, 1, 1);
cairo_move_to(cr, 150, 250);
cairo_show_text(cr, "meniOS");
```

#### 2. Pixman (#397)
**Purpose:** Low-level pixel manipulation and software rasterization (Cairo dependency)

**Capabilities:**
- Porter-Duff compositing operators (SRC, OVER, ADD, etc.)
- Bilinear and bicubic image filtering
- Image transformations (rotation, scaling, affine)
- SIMD optimizations (SSE2, SSSE3, AVX2) for x86-64

**Integration:**
- Required by Cairo for all pixel-level operations
- Handles actual rasterization of vector paths to pixels
- Optimized fast paths for common operations
- Located in `vendor/pixman-0.42.2/`

#### 3. FreeType (#398)
**Purpose:** Font rendering library (Cairo dependency for text)

**Capabilities:**
- TrueType (TTF) and OpenType (OTF) font loading
- Glyph rasterization with hinting for small sizes
- Subpixel rendering (LCD optimization)
- Unicode character mapping

**Integration:**
- Cairo FT font backend uses FreeType for text rendering
- Fonts stored in `/usr/share/fonts/` (e.g., DejaVu Sans, Liberation Sans)
- Glyph cache maintained for performance
- Located in `vendor/freetype-2.13.2/`

#### 4. Compositor Core (#403)
**Purpose:** Central display server managing all windows and screen output

**Responsibilities:**
- Accept client connections via Unix domain sockets (#402)
- Manage window surfaces allocated in shared memory (#401)
- Composite windows in Z-order to screen framebuffer
- Route input events to appropriate windows
- Implement damage tracking for efficient redraws

**Key Data Structures:**
```c
// Compositor state (src/userland/compositor/compositor.c)
struct compositor {
    cairo_surface_t *screen;       // Framebuffer surface (1024x768 ARGB32)
    struct window *windows;         // Doubly-linked window list (Z-order)
    struct window *focused;         // Currently focused window
    struct event_queue input_queue; // Keyboard/mouse events
    int server_socket;              // AF_UNIX listening socket
    struct pollfd *client_fds;      // Connected clients
    bool damage_dirty;              // Screen needs repaint
};

// Window representation
struct window {
    uint32_t id;                    // Unique window ID
    struct client *client;          // Owning client connection

    // Geometry
    int x, y;                       // Position on screen
    int width, height;              // Window dimensions

    // State
    bool mapped;                    // Window is visible
    bool focused;                   // Has keyboard focus

    // Rendering
    cairo_surface_t *surface;       // Shared memory surface
    struct {
        int x, y, width, height;    // Dirty region
        bool valid;
    } damage;

    // Decoration state
    char title[256];                // Window title
    bool has_decorations;           // Client-side or server-side

    // Z-order links
    struct window *above, *below;
};
```

**Compositor Main Loop:**
```c
while (compositor_running) {
    // 1. Process client protocol messages
    compositor_handle_client_messages(comp);

    // 2. Dispatch input events to windows
    compositor_dispatch_input(comp);

    // 3. Render if damaged
    if (comp->damage_dirty) {
        compositor_render(comp);
        comp->damage_dirty = false;
    }

    // 4. Wait for next event (with vsync timeout)
    poll(comp->client_fds, comp->num_clients, 16);  // 60fps = 16.67ms
}
```

#### 5. Window Protocol (#404)
**Purpose:** IPC protocol between clients and compositor

**Transport:** Unix domain sockets (AF_UNIX, SOCK_STREAM) over `/tmp/menios-compositor`

**Message Format:**
```c
// Client → Compositor (requests)
enum request_type {
    REQ_CREATE_SURFACE,    // Create new window
    REQ_DESTROY_SURFACE,   // Destroy window
    REQ_ATTACH_BUFFER,     // Attach shared memory buffer
    REQ_DAMAGE,            // Mark region dirty
    REQ_COMMIT,            // Apply pending state
    REQ_SET_TITLE,         // Set window title
    REQ_FRAME_CALLBACK,    // Request vsync notification
};

struct request {
    uint32_t type;
    uint32_t surface_id;
    union {
        struct {
            int width, height;
        } create;
        struct {
            int fd;           // Shared memory fd (passed via SCM_RIGHTS)
            int width, height;
            int stride;
        } attach;
        struct {
            int x, y, width, height;
        } damage;
        struct {
            char title[256];
        } set_title;
    };
};

// Compositor → Client (events)
enum event_type {
    EVT_CONFIGURE,         // Compositor requests size change
    EVT_KEY_PRESS,         // Keyboard key pressed
    EVT_KEY_RELEASE,       // Keyboard key released
    EVT_MOUSE_MOTION,      // Mouse moved
    EVT_MOUSE_BUTTON,      // Mouse button clicked
    EVT_FRAME_DONE,        // Vsync callback
    EVT_CLOSE,             // Window close requested
};

struct event {
    uint32_t type;
    uint32_t surface_id;
    union {
        struct {
            int width, height;
        } configure;
        struct {
            uint32_t keycode;
            uint32_t modifiers;  // Shift, Ctrl, Alt
        } key;
        struct {
            int x, y;            // Window-relative coordinates
        } mouse_motion;
        struct {
            uint32_t button;     // 1=left, 2=middle, 3=right
            int x, y;
        } mouse_button;
    };
};
```

**Protocol Flow Example:**
```
Client                          Compositor
  |                                 |
  |--- REQ_CREATE_SURFACE --------->|
  |<-- EVT_CONFIGURE(800x600) ------|
  |                                 |
  | shm_open("/window_42")          |
  | mmap() shared buffer            |
  | cairo_create_for_data()         |
  | [render content to buffer]      |
  |                                 |
  |--- REQ_ATTACH_BUFFER(fd) ------>| (fd passed via SCM_RIGHTS)
  |--- REQ_DAMAGE(0,0,800,600) ---->|
  |--- REQ_COMMIT ------------------>|
  |                                 | [composites to screen]
  |<-- EVT_FRAME_DONE --------------|
  |                                 |
  | [user clicks window]            |
  |<-- EVT_MOUSE_BUTTON(1, x, y) ---|
```

#### 6. Window Manager (#406, #407, #408)
**Purpose:** Manage window placement, decorations, and user interactions

**Integrated into compositor** rather than separate process (Wayland-style design).

**Window Placement Policies:**
```c
// Cascade placement (default)
void wm_place_cascade(struct window *w) {
    static int offset = 0;
    w->x = 100 + offset;
    w->y = 80 + offset;
    offset = (offset + 30) % 300;
}

// Centered placement
void wm_place_centered(struct window *w) {
    w->x = (screen_width - w->width) / 2;
    w->y = (screen_height - w->height) / 2;
}

// Tiling (future)
void wm_place_tiled(struct window *w) {
    // Automatic grid layout
}
```

**Window Operations:**
```c
void wm_move_window(struct window *w, int x, int y);
void wm_resize_window(struct window *w, int width, int height);
void wm_raise_window(struct window *w);     // Bring to front
void wm_lower_window(struct window *w);     // Send to back
void wm_minimize_window(struct window *w);  // Hide from screen
void wm_maximize_window(struct window *w);  // Expand to full screen
void wm_close_window(struct window *w);     // Request close
```

**Focus Management (#407):**
- **Click-to-focus**: Mouse click on window brings to front and gives keyboard focus
- **Focus-follows-mouse** (optional): Hovering over window gives focus
- **Tab cycling**: Alt+Tab switches focus between windows
- Visual feedback: Focused window has blue title bar, unfocused windows have gray

**Window Decorations (#408):**
```c
#define TITLEBAR_HEIGHT 24
#define BORDER_WIDTH 1

void wm_draw_decorations(cairo_t *cr, struct window *w) {
    // Title bar
    cairo_rectangle(cr, w->x, w->y - TITLEBAR_HEIGHT, w->width, TITLEBAR_HEIGHT);
    if (w->focused) {
        cairo_set_source_rgb(cr, 0.2, 0.4, 0.7);  // Blue
    } else {
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);  // Gray
    }
    cairo_fill(cr);

    // Title text
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, w->x + 5, w->y - 8);
    cairo_show_text(cr, w->title);

    // Close button
    int btn_x = w->x + w->width - 18;
    int btn_y = w->y - 20;
    cairo_rectangle(cr, btn_x, btn_y, 14, 14);
    cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);  // Red
    cairo_fill(cr);

    // Border
    cairo_rectangle(cr, w->x - BORDER_WIDTH, w->y - TITLEBAR_HEIGHT - BORDER_WIDTH,
                    w->width + 2*BORDER_WIDTH, w->height + TITLEBAR_HEIGHT + 2*BORDER_WIDTH);
    cairo_set_line_width(cr, BORDER_WIDTH);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_stroke(cr);
}
```

#### 7. Input Event System (#399)
**Purpose:** Unified abstraction for keyboard, mouse, and future touch input

**Event Queue:**
```c
#define EVENT_QUEUE_SIZE 256

struct input_event {
    uint64_t timestamp;       // Microseconds since boot
    enum {
        INPUT_KEY_PRESS,
        INPUT_KEY_RELEASE,
        INPUT_MOUSE_MOTION,
        INPUT_MOUSE_BUTTON_PRESS,
        INPUT_MOUSE_BUTTON_RELEASE,
        INPUT_MOUSE_SCROLL,
    } type;
    union {
        struct {
            uint32_t keycode;    // PS/2 scan code
            uint32_t unicode;    // UTF-32 codepoint
            uint32_t modifiers;  // Shift, Ctrl, Alt, etc.
        } key;
        struct {
            int32_t x, y;        // Absolute screen coordinates
            int32_t dx, dy;      // Relative motion
        } motion;
        struct {
            uint32_t button;     // 1=left, 2=middle, 3=right
            int32_t x, y;
        } button;
        struct {
            int32_t dx, dy;      // Scroll wheel delta
        } scroll;
    };
};

struct event_queue {
    struct input_event events[EVENT_QUEUE_SIZE];
    size_t head, tail;
    struct mutex lock;
    struct semaphore sem;
};

int event_queue_push(struct event_queue *q, struct input_event *evt);
int event_queue_pop(struct event_queue *q, struct input_event *evt);
```

**Integration with Drivers:**
```c
// PS/2 keyboard driver (src/kernel/drivers/input/ps2_keyboard.c)
void ps2_keyboard_interrupt(struct registers *regs) {
    uint8_t scancode = inb(0x60);

    struct input_event evt = {
        .timestamp = get_timestamp_us(),
        .type = (scancode & 0x80) ? INPUT_KEY_RELEASE : INPUT_KEY_PRESS,
        .key = {
            .keycode = scancode & 0x7F,
            .unicode = scancode_to_unicode(scancode),
            .modifiers = get_modifiers(),
        }
    };

    event_queue_push(&global_event_queue, &evt);
}

// PS/2 mouse driver (src/kernel/drivers/input/ps2_mouse.c) - #143
void ps2_mouse_interrupt(struct registers *regs) {
    static struct {
        uint8_t buttons;
        int16_t dx, dy;
    } mouse_state;

    // Parse PS/2 mouse packet (3 bytes)
    parse_ps2_packet(&mouse_state);

    struct input_event evt = {
        .timestamp = get_timestamp_us(),
        .type = INPUT_MOUSE_MOTION,
        .motion = {
            .x = cursor_x + mouse_state.dx,
            .y = cursor_y + mouse_state.dy,
            .dx = mouse_state.dx,
            .dy = mouse_state.dy,
        }
    };

    event_queue_push(&global_event_queue, &evt);
}
```

#### 8. Mouse Cursor (#400)
**Purpose:** Render hardware or software cursor on screen

**Software Cursor Implementation:**
```c
struct cursor {
    enum {
        CURSOR_ARROW,
        CURSOR_HAND,
        CURSOR_TEXT,
        CURSOR_CROSSHAIR,
        CURSOR_WAIT,
        CURSOR_RESIZE_NS,
        CURSOR_RESIZE_EW,
    } type;

    uint32_t *pixels;          // ARGB32 cursor image
    int width, height;         // Cursor dimensions (e.g., 16x16)
    int hotspot_x, hotspot_y;  // Click point (e.g., arrow tip)
};

void cursor_render(cairo_t *cr, struct cursor *cursor, int x, int y) {
    cairo_surface_t *cursor_surface = cairo_image_surface_create_for_data(
        (unsigned char*)cursor->pixels,
        CAIRO_FORMAT_ARGB32,
        cursor->width, cursor->height,
        cursor->width * 4
    );

    cairo_set_source_surface(cr, cursor_surface,
                             x - cursor->hotspot_x,
                             y - cursor->hotspot_y);
    cairo_rectangle(cr, x - cursor->hotspot_x, y - cursor->hotspot_y,
                    cursor->width, cursor->height);
    cairo_fill(cr);

    cairo_surface_destroy(cursor_surface);
}
```

**Hit Testing:**
```c
struct window *compositor_window_at(struct compositor *comp, int x, int y) {
    // Iterate windows from top to bottom (reverse Z-order)
    for (struct window *w = comp->windows; w; w = w->below) {
        if (!w->mapped) continue;

        // Check if point is in window (including decorations)
        int wx1 = w->x;
        int wy1 = w->y - TITLEBAR_HEIGHT;
        int wx2 = w->x + w->width;
        int wy2 = w->y + w->height;

        if (x >= wx1 && x < wx2 && y >= wy1 && y < wy2) {
            return w;
        }
    }
    return NULL;  // Desktop background
}
```

#### 9. Shared Memory (#401)
**Purpose:** Zero-copy window buffer sharing between clients and compositor

**POSIX Shared Memory API:**
```c
// Client creates shared buffer
int fd = shm_open("/menios_window_123", O_CREAT | O_RDWR, 0600);
ftruncate(fd, width * height * 4);  // ARGB32 = 4 bytes/pixel

void *pixels = mmap(NULL, width * height * 4,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);

// Client renders to buffer
cairo_surface_t *surface = cairo_image_surface_create_for_data(
    pixels, CAIRO_FORMAT_ARGB32, width, height, width * 4);
cairo_t *cr = cairo_create(surface);
// ... render content ...

// Send fd to compositor via Unix socket SCM_RIGHTS
struct msghdr msg = {0};
struct cmsghdr *cmsg;
char cmsgbuf[CMSG_SPACE(sizeof(int))];
msg.msg_control = cmsgbuf;
msg.msg_controllen = sizeof(cmsgbuf);

cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type = SCM_RIGHTS;
cmsg->cmsg_len = CMSG_LEN(sizeof(int));
memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

sendmsg(compositor_socket, &msg, 0);
```

**Compositor maps same buffer:**
```c
// Compositor receives fd via SCM_RIGHTS
int client_fd;
recvmsg(client_socket, &msg, 0);
memcpy(&client_fd, CMSG_DATA(cmsg), sizeof(int));

// Map into compositor's address space
void *compositor_pixels = mmap(NULL, width * height * 4,
                               PROT_READ,  // Read-only for compositor
                               MAP_SHARED, client_fd, 0);

// Create Cairo surface pointing to same memory
cairo_surface_t *window_surface = cairo_image_surface_create_for_data(
    compositor_pixels, CAIRO_FORMAT_ARGB32, width, height, width * 4);

// Compositor can now composite this surface without copying
```

#### 10. Desktop Shell (#409)
**Purpose:** Desktop environment (panel, launcher, background)

**Components:**
- **Desktop background**: Solid color or wallpaper image
- **Panel/Taskbar**: Top or bottom bar with:
  - Application launcher button
  - Window list (shows all open windows)
  - System tray (clock, volume, network, battery)
- **Application launcher menu**: Grid or list of installed applications
- **Notification area**: Popup notifications (future)

**Implementation:**
```c
// Desktop shell runs as special compositor client
struct desktop_shell {
    struct window *panel;       // Panel window (full width, 28px tall)
    struct window *background;  // Desktop background (full screen)
    struct window *launcher;    // Application launcher menu (popup)
};

void shell_draw_panel(cairo_t *cr) {
    int panel_height = 28;

    // Panel background
    cairo_rectangle(cr, 0, 0, screen_width, panel_height);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_fill(cr);

    // Application launcher button
    draw_button(cr, 5, 2, 100, 24, "Applications");

    // Window list
    int x = 120;
    for (struct window *w = compositor->windows; w; w = w->next) {
        if (!w->mapped || w == panel) continue;
        draw_button(cr, x, 2, 120, 24, w->title);
        x += 125;
    }

    // Clock (right side)
    char time_str[32];
    time_t now = time(NULL);
    strftime(time_str, sizeof(time_str), "%H:%M", localtime(&now));

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, screen_width - 60, 18);
    cairo_show_text(cr, time_str);
}
```

## Integration Points

- **Compositor**: `src/userland/compositor/` - Main display server daemon
  - `compositor.c` - Core compositor logic
  - `window.c` - Window management
  - `protocol.c` - Client protocol handling
  - `input.c` - Input event dispatch

- **Desktop Shell**: `src/userland/desktop/` - Desktop environment
  - `shell.c` - Panel and launcher
  - `panel.c` - Taskbar rendering
  - `launcher.c` - Application menu

- **Client Library**: `src/userland/libgui/` - Shared library for GUI apps
  - `libgui.c` - Window creation, event handling
  - `libgui.h` - Public API for applications

- **Cairo Integration**: `vendor/cairo-1.18.0/`
  - `cairo-menios.c` - Framebuffer backend implementation
  - Patches for meniOS-specific features

- **Example Applications**: `app/gui/`
  - `terminal.c` - Terminal emulator
  - `editor.c` - Text editor
  - `filemanager.c` - File browser
  - `calculator.c` - Calculator

## Performance Considerations

### Damage Tracking
Compositor only redraws changed regions rather than entire screen:
```c
void compositor_damage(struct compositor *comp, int x, int y, int w, int h) {
    comp->damage_region = rect_union(comp->damage_region,
                                     rect(x, y, w, h));
    comp->damage_dirty = true;
}

void compositor_render(struct compositor *comp) {
    if (!comp->damage_dirty) return;

    // Only redraw damaged region
    cairo_rectangle(cr, comp->damage_region.x, comp->damage_region.y,
                   comp->damage_region.width, comp->damage_region.height);
    cairo_clip(cr);

    // Composite windows
    for (struct window *w = comp->windows; w; w = w->next) {
        if (!w->mapped) continue;
        if (!rect_intersects(w->bounds, comp->damage_region)) continue;

        cairo_set_source_surface(cr, w->surface, w->x, w->y);
        cairo_paint(cr);
    }

    comp->damage_dirty = false;
}
```

### Double Buffering
Compositor uses double buffering to prevent tearing:
```c
struct compositor {
    cairo_surface_t *front_buffer;  // Currently displayed
    cairo_surface_t *back_buffer;   // Being rendered to
};

void compositor_swap_buffers(struct compositor *comp) {
    cairo_surface_t *tmp = comp->front_buffer;
    comp->front_buffer = comp->back_buffer;
    comp->back_buffer = tmp;

    // Blit front buffer to /dev/fb0
    memcpy(fb_mem, cairo_image_surface_get_data(comp->front_buffer),
           screen_width * screen_height * 4);
}
```

### Vsync Synchronization
Rendering synchronized to vertical refresh (60Hz):
```c
void compositor_wait_vsync(struct compositor *comp) {
    // Option 1: ioctl on /dev/fb0
    ioctl(fb_fd, FBIO_WAITFORVSYNC, 0);

    // Option 2: Timer-based (16.67ms for 60fps)
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 16666667 };
    nanosleep(&ts, NULL);
}
```

## Security Model

- **Input isolation**: Only focused window receives keyboard events
- **Credential passing**: Compositor verifies client credentials via Unix socket SCM_CREDENTIALS
- **Sandboxing** (future): Clients run in restricted environment (seccomp, namespaces)
- **No global keyboard snooping**: Unlike X11, clients cannot spy on other windows

## Testing Strategy

### Unit Tests
- Window manager placement algorithms
- Damage tracking region calculations
- Event queue operations
- Protocol message encoding/decoding

### Integration Tests
- Multi-window rendering
- Client connection lifecycle
- Input event routing
- Shared memory buffer mapping

### Visual Tests
- Render test patterns to validate anti-aliasing
- Font rendering quality across sizes
- Window decoration consistency
- Cursor tracking accuracy

### Performance Tests
- Measure frame rate during window movement
- Benchmark composition time for 10+ windows
- Test damage tracking efficiency
- Profile Cairo rendering operations

## Risks and Open Questions

### Memory Footprint
- **Problem**: Multiple Cairo surfaces (one per window) consume significant RAM
- **Mitigation**: Limit total window surfaces, implement surface eviction for minimized windows
- **Metrics**: Monitor via `menios_malloc_stats()`, aim for <64MB total for 10 windows

### Threading Model
- **Question**: Should compositor be single-threaded or multi-threaded?
- **Current**: Single-threaded event loop (simpler, less race conditions)
- **Future**: Could parallelize window rendering if needed
- **Dependency**: #109 (pthread API), #339 (thread-safe libc)

### GPU Acceleration
- **Current**: Software rendering via Cairo/Pixman
- **Performance**: Adequate for basic desktop (aiming for 60fps at 1024x768)
- **Future**: OpenGL/Vulkan backend for hardware acceleration
- **Blocker**: Requires GPU driver development (Intel i915, AMD, etc.)

### Font Rendering Quality
- **Question**: Software rendering vs hardware acceleration for text?
- **Current**: FreeType software rasterization with subpixel anti-aliasing
- **Performance**: 10,000+ glyphs/sec should be sufficient
- **Tuning**: Font caching critical for editor/terminal performance

### Window Protocol Versioning
- **Problem**: How to evolve protocol without breaking clients?
- **Solution**: Protocol version negotiation during handshake
- **Example**: Client sends `HELLO(version=1)`, compositor responds with supported version

## Future Extensions

- **Clipboard support**: Copy/paste between applications
- **Drag and drop**: File and window dragging
- **Notifications**: Desktop notification API (similar to libnotify)
- **3D rendering**: OpenGL/Vulkan context creation for games
- **Accessibility**: Screen reader, magnifier, high-contrast themes
- **Multi-monitor**: Support for multiple displays
- **Network transparency**: Remote application display (similar to X forwarding)
- **Widget toolkit**: Higher-level UI library (buttons, text boxes, menus)

## References

- **Wayland Protocol Specification**: https://wayland.freedesktop.org/docs/html/
- **Cairo Graphics Manual**: https://www.cairographics.org/manual/
- **Weston Compositor Source**: https://gitlab.freedesktop.org/wayland/weston
- **SerenityOS LibGUI**: https://github.com/SerenityOS/serenity/tree/master/Userland/Libraries/LibGUI
- **ToaruOS Compositor**: https://github.com/klange/toaruos/tree/master/apps

## Document History

- **2025-10-30**: Initial version documenting GUI architecture decisions
