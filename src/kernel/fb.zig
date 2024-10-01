const fonts = @import("fonts.zig");

var current_col: u32 = 0;
var current_row: u32 = 0;

const char_line_width = 8;
const char_line_height = 16;
const FB_WHITE = 0x7f7f7f;
const ROWS = 50;
const COLS = 140;

extern fn fb_putpixel(x: u32, y: u32, rgb: u32) void;

pub export fn fb_putchar(c: i32) i32 {
    if (c == 0) {
        return 0;
    }

    if (c == '\n') {
        current_col = 0;
        current_row += 1;
        return 0;
    }

    const glyph = fonts.font_glyph(@intCast(c));

    const x = (current_col * char_line_width) + 10;
    const y = (current_row * char_line_height) + 10;

    for (0..char_line_width) |gx| {
        for (0..char_line_height) |gy| {
            if (((glyph.points[gy] >> @intCast(7 - gx)) & 0x01) != 0) {
                fb_putpixel(@intCast(x + gx), @intCast(y + gy), FB_WHITE);
            }
        }
    }
    if (current_col < COLS) {
        current_col += 1;
    } else {
        current_col = 0;
        current_row += 1;
    }
    return 0;
}
