font = {
  :glyphs => []
}

current_glyph = {}
bitmap = FALSE
skip = FALSE

File.readlines("/Users/pliniobalduino-mba/Downloads/terminus-font-4.49.1/ter-u16n.bdf").each do |line|

  if line.start_with?("STARTCHAR")
    current_glyph = {}

  elsif line.start_with?("ENCODING")
    code = line.split(" ")[1].to_i
    skip = code > 255
    current_glyph[:code] = line.split(" ")[1].to_i if !skip

  elsif line.start_with?("BITMAP") && !skip
    current_glyph[:lines] = []
    bitmap = TRUE

  elsif line.start_with?("ENDCHAR")
    if !skip
      font[:glyphs] << current_glyph
      bitmap = FALSE
    end

    skip = FALSE

  elsif bitmap && !skip
    current_glyph[:lines] << line.to_i

  end

end

i = 1
File.open("font.c", "w") do |f|
  f.puts("#include <types.h>\n\n")

  f.printf("static void* font_terminus = {0x%0x, ", font[:glyphs].size)

  font[:glyphs].each do |glyph|
    f.printf("0x%02x, ", glyph[:code])
    f.print("\n  ") if (i % 16) == 0
    i += 1

    glyph[:lines].each do |l|
      f.printf("0x%02x, ", l)
      f.print("\n  ") if (i % 16) == 0
      i += 1
    end

  end

  (1..16).each do |i|
    f.print "0xff"
    f.print ", " if i < 16
  end

  f.puts "\n};"
end
