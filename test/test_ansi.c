#include <kernel/ansi.h>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_colors(uint8_t fg_index, uint8_t bg_index, uint8_t attrs, uint32_t expected_fg, uint32_t expected_bg) {
  ansi_style_t style = {.fg = fg_index, .bg = bg_index, .attrs = attrs};
  uint32_t fg;
  uint32_t bg;
  ansi_style_effective_colors(&style, &fg, &bg);
  TEST_ASSERT_EQUAL_HEX32(expected_fg, fg);
  TEST_ASSERT_EQUAL_HEX32(expected_bg, bg);
}

void test_ansi_reset_sets_white_on_black(void) {
  ansi_style_t style = {.fg = 3, .bg = 4, .attrs = 0xff};
  ansi_style_reset(&style);
  TEST_ASSERT_EQUAL_UINT8(7, style.fg);
  TEST_ASSERT_EQUAL_UINT8(0, style.bg);
  TEST_ASSERT_EQUAL_UINT8(0, style.attrs);
}

void test_ansi_bold_promotes_to_bright_variant(void) {
  ansi_style_t style;
  ansi_style_reset(&style);
  int params[] = {31, 1};
  ansi_style_apply_sgr(&style, params, 2);
  TEST_ASSERT_EQUAL_UINT8(1, style.fg);
  TEST_ASSERT_EQUAL_UINT8(0, style.bg);
  TEST_ASSERT_TRUE(style.attrs & ANSI_ATTR_BOLD);

  uint32_t fg;
  uint32_t bg;
  ansi_style_effective_colors(&style, &fg, &bg);
  TEST_ASSERT_EQUAL_HEX32(ansi_palette_color(9), fg);
  TEST_ASSERT_EQUAL_HEX32(ansi_palette_color(0), bg);
}

void test_ansi_dim_clears_bold_and_darksen_color(void) {
  ansi_style_t style;
  ansi_style_reset(&style);
  int params[] = {32, 1, 2};
  ansi_style_apply_sgr(&style, params, 3);
  uint32_t fg;
  uint32_t bg;
  ansi_style_effective_colors(&style, &fg, &bg);
  TEST_ASSERT_EQUAL_UINT8(2, style.fg);
  TEST_ASSERT_EQUAL_HEX32(ansi_dim_color(ansi_palette_color(2)), fg);
  TEST_ASSERT_EQUAL_HEX32(ansi_palette_color(0), bg);
  TEST_ASSERT_TRUE(style.attrs & ANSI_ATTR_DIM);
  TEST_ASSERT_FALSE(style.attrs & ANSI_ATTR_BOLD);
}

void test_ansi_reverse_swaps_colors(void) {
  assert_colors(4, 7, ANSI_ATTR_REVERSE, ansi_palette_color(7), ansi_palette_color(4));
}

void test_ansi_color_selection_handles_extended_ranges(void) {
  ansi_style_t style;
  ansi_style_reset(&style);
  int params[] = {94, 102};
  ansi_style_apply_sgr(&style, params, 2);
  TEST_ASSERT_EQUAL_UINT8( (uint8_t)(8 + (94 - 90)), style.fg);
  TEST_ASSERT_EQUAL_UINT8( (uint8_t)(8 + (102 - 100)), style.bg);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_ansi_reset_sets_white_on_black);
  RUN_TEST(test_ansi_bold_promotes_to_bright_variant);
  RUN_TEST(test_ansi_dim_clears_bold_and_darksen_color);
  RUN_TEST(test_ansi_reverse_swaps_colors);
  RUN_TEST(test_ansi_color_selection_handles_extended_ranges);

  return UNITY_END();
}
