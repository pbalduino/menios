const std = @import("std");

extern fn serial_printf(format: [*c]const u8, ...) c_int;

pub const glypht_t = struct {
    value: u8, // 'char' in C typically corresponds to 'u8' in Zig
    points: [16]u8, // uint8_t array of size 16
};

pub const font_t = struct {
    glyphs: [0x100]glypht_t, // Array of 256 glypht_t structs
};

pub const GL_NULL = glypht_t{
    .value = 0x00,
    .points = [16]u8{ 0x00, 0x00, 0x66, 0x42, 0x00, 0x42, 0x42, 0x42, 0x00, 0x42, 0x42, 0x66, 0x00, 0x00, 0x00, 0x00 },
};

pub const font_list: font_t = font_t{
    .glyphs = [_]glypht_t{GL_NULL} ** 0x100,
};

pub fn font_init() void {
    _ = serial_printf("font_init: Initing Z\n");

    // for (font_list.glyphs) |glyph| {
    //     glyph = GL_NULL;
    // }
}

pub fn font_glyph(ascii: u8) glypht_t {
    return font_list.glyphs[ascii];
}
