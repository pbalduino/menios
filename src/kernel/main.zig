const fonts = @import("fonts.zig");
const fb = @import("fb.zig");

extern fn fb_bpp() u64;
extern fn fb_height() u64;
extern fn fb_init() void;
extern fn fb_mode_count() u64;
extern fn fb_width() u64;

extern fn file_init() void;

extern fn printf(format: [*c]const u8, ...) c_int;
extern fn putchar(char: usize) c_int;
extern fn puts(text: [*c]const u8) c_int;

extern fn serial_init() void;

pub fn main() void {
    _start();
}

fn boot_graphics_init() void {
    fb_init();
    fonts.font_init();

    _ = puts("Welcome to meniOS 0.0.4 64bits\n\n- Typeset test:");

    for (32..128) |c| {
        if (c % 32 == 0) {
            _ = puts("\n ");
        }

        _ = putchar(c);
        _ = putchar(' ');
    }
    _ = putchar('\n');
    _ = printf("- Screen mode: %lu x %lu x %d\n", fb_width(), fb_height(), fb_bpp());
    _ = printf("  Available modes: %lu\n", fb_mode_count());
    // fb_list_modes();
}

export fn _start() void {
    file_init();

    serial_init();

    boot_graphics_init();

    _ = fb.fb_putchar(' ');
}
