#include <kernel/console.h>

#include <string.h>
#include <unity.h>

void setUp() {
}

void tearDown() {

}
/*
%[flags][min field width][precision][length]conversion specifier
  -----  ---------------  ---------  ------ -------------------
   \             #,*        .#, .*     /             \
    \                                 /               \
   #,0,-,+, ,',I                 hh,h,l,ll,j,z,L    c,d,u,x,X,e,f,g,s,p,%
   -------------                 ---------------    -----------------------
   # | Alternate,                 hh | char,           c | unsigned char,
   0 | zero pad,                   h | short,          d | signed int,
   - | left align,                 l | long,           u | unsigned int,
   + | explicit + - sign,         ll | long long,      x | unsigned hex int,
     | space for + sign,           j | [u]intmax_t,    X | unsigned HEX int,
   ' | locale thousands grouping,  z | size_t,         e | [-]d.ddde±dd double,
   I | Use locale's alt digits     t | ptrdiff_t,      E | [-]d.dddE±dd double,
                                   L | long double,  ---------=====
   if no precision   => 6 decimal places            /  f | [-]d.ddd double,
   if precision = 0  => 0 decimal places      _____/   g | e|f as appropriate,
   if precision = #  => # decimal places               G | E|F as appropriate,
   if flag = #       => always show decimal point      s | string,
                                             ..............------
                                            /          p | pointer,
   if precision      => max field width    /           % | %

Source: https://www.pixelbeat.org/programming/gcc/format_specs.html
*/
// NOTE: the kernel has no support to floating point, so it won't implement e, E, f, g, G

// TODO: test simple print, with no format
void test_vprintk_WHEN_no_percent_SHOULD_return_simple_text() {
  char buffer[256];

  vprintk(buffer, "simple text");

  TEST_ASSERT_EQUAL_STRING("simple text", buffer);
}

// TODO: test simple %c
void test_vprintk_WHEN_percent_c_SHOULD_return_a_character() {
  char buffer[256];

  vprintk(buffer, "%c", 'A');

  TEST_ASSERT_EQUAL_STRING("A", buffer);
}

// TODO: test simple %d
void test_vprintk_WHEN_percent_d_SHOULD_return_a_positive_number() {
  char buffer[256];
  vprintk(buffer, "%d", 123);

  TEST_ASSERT_EQUAL_STRING("123", buffer);
}

// TODO: test simple %d
void test_vprintk_WHEN_percent_d_SHOULD_return_a_negative_number() {
  char buffer[256];
  vprintk(buffer, "%d", -123);

  TEST_ASSERT_EQUAL_STRING("-123", buffer);
}

// TODO: test simple %u
// TODO: test simple %x
// TODO: test simple %X
// TODO: test simple %s
// TODO: test simple %p
// TODO: test simple %%
// TODO: test left padded number with spaces
void test_vprintk_WHEN_percent_5d_SHOULD_return_a_space_padded_number() {
  char buffer[256];
  vprintk(buffer, "%5d", 123);
  TEST_ASSERT_EQUAL_STRING("  123", buffer);

  vprintk(buffer, "%5d", -123);
  TEST_ASSERT_EQUAL_STRING(" -123", buffer);

  vprintk(buffer, "%5d", 12345);
  TEST_ASSERT_EQUAL_STRING("12345", buffer);

  vprintk(buffer, "%5d", 123456);
  TEST_ASSERT_EQUAL_STRING("123456", buffer);

  vprintk(buffer, "%10d", 123456);
  TEST_ASSERT_EQUAL_STRING("    123456", buffer);
}

// TODO: test left padded character with spaces
void test_vprintk_WHEN_percent_5c_SHOULD_return_a_space_padded_character() {
  char buffer[256];

  vprintk(buffer, "%5c", 'A');
  TEST_ASSERT_EQUAL_STRING("    A", buffer);

  vprintk(buffer, "%10c", 'A');
  TEST_ASSERT_EQUAL_STRING("         A", buffer);

}

void test_vprintk_WHEN_percent_05c_SHOULD_return_a_space_padded_character() {
  char buffer[256];

  vprintk(buffer, "%05c", 'A');
  TEST_ASSERT_EQUAL_STRING("0000A", buffer);

  vprintk(buffer, "%010c", 'A');
  TEST_ASSERT_EQUAL_STRING("000000000A", buffer);

}

// TODO: test left padded number with zero
void test_vprintk_WHEN_percent_05d_SHOULD_return_a_zero_padded_number() {
  char buffer[256];
  vprintk(buffer, "%05d", 123);
  TEST_ASSERT_EQUAL_STRING("00123", buffer);

  vprintk(buffer, "%05d", -123);
  TEST_ASSERT_EQUAL_STRING("-0123", buffer);

  vprintk(buffer, "%05d", 12345);
  TEST_ASSERT_EQUAL_STRING("12345", buffer);

  vprintk(buffer, "%05d", 123456);
  TEST_ASSERT_EQUAL_STRING("123456", buffer);

  vprintk(buffer, "%010d", 123456);
  TEST_ASSERT_EQUAL_STRING("0000123456", buffer);
}

void test_vprintk_WHEN_percent_u_SHOULD_return_unsigned_integer() {
  char buffer[256];
  vprintk(buffer, "%u", 12345);
  TEST_ASSERT_EQUAL_STRING("12345", buffer);

  vprintk(buffer, "%u", 0);
  TEST_ASSERT_EQUAL_STRING("0", buffer);
}

void test_vprintk_WHEN_percent_x_SHOULD_return_lowercase_hex() {
  char buffer[256];
  vprintk(buffer, "%x", 255);
  TEST_ASSERT_EQUAL_STRING("ff", buffer);
}

void test_vprintk_WHEN_percent_X_SHOULD_return_uppercase_hex() {
  char buffer[256];
  vprintk(buffer, "%X", 255);
  TEST_ASSERT_EQUAL_STRING("FF", buffer);
}

void test_vprintk_WHEN_percent_s_SHOULD_return_string() {
  char buffer[256];
  vprintk(buffer, "%s", "Hello, World!");
  TEST_ASSERT_EQUAL_STRING("Hello, World!", buffer);
}

void test_vprintk_WHEN_percent_p_SHOULD_return_pointer_address() {
  char buffer[256];
  int *x = (int*)0x1000;
  vprintk(buffer, "%p", x);
  TEST_ASSERT_EQUAL_STRING("0x1000", buffer);
}

void test_vprintk_WHEN_percent_percent_SHOULD_return_percent_sign() {
  char buffer[256];
  vprintk(buffer, "100%%");
  TEST_ASSERT_EQUAL_STRING("100%", buffer);
}

void test_vprintk_WHEN_percent_minus_SHOULD_left_align() {
  char buffer[256];
  vprintk(buffer, "%-5d", 123);
  TEST_ASSERT_EQUAL_STRING("123  ", buffer);
}

void test_vprintk_WHEN_precision_SHOULD_limit_string_length() {
  char buffer[256];
  vprintk(buffer, "%.3s", "abcdef");
  TEST_ASSERT_EQUAL_STRING("abc", buffer);
}

void test_vprintk_WHEN_multiple_specifiers_SHOULD_format_correctly() {
  char buffer[256];
  vprintk(buffer, "%d %u %x %c", 123, 456, 255, 'A');
  TEST_ASSERT_EQUAL_STRING("123 456 ff A", buffer);
}

void test_vprintk_WHEN_width_and_precision_SHOULD_format_correctly() {
  char buffer[256];
  vprintk(buffer, "%8.3d", 12345);
  TEST_ASSERT_EQUAL_STRING("    123", buffer);
}

void test_vprintk_WHEN_large_number_SHOULD_print_correctly() {
  char buffer[256];
  vprintk(buffer, "%d", 2147483647);  // INT_MAX
  TEST_ASSERT_EQUAL_STRING("2147483647", buffer);
}

void test_vprintk_WHEN_small_number_SHOULD_print_correctly() {
  char buffer[256];
  vprintk(buffer, "%d", -2147483648);  // INT_MIN
  TEST_ASSERT_EQUAL_STRING("-2147483648", buffer);
}

void test_vprintk_WHEN_unsigned_long_SHOULD_print_correctly() {
  char buffer[256];
  vprintk(buffer, "%lu", 4294967295UL);  // ULONG_MAX
  TEST_ASSERT_EQUAL_STRING("4294967295", buffer);
}

void test_vprintk_WHEN_long_long_SHOULD_print_correctly() {
  char buffer[256];
  vprintk(buffer, "%lld", 9223372036854775807LL);  // LLONG_MAX
  TEST_ASSERT_EQUAL_STRING("9223372036854775807", buffer);
}

void test_vprintk_WHEN_multiple_args_SHOULD_print_correctly() {
  char buffer[256];
  vprintk(buffer, "%d %s %c", 42, "test", 'X');
  TEST_ASSERT_EQUAL_STRING("42 test X", buffer);
}

void test_vprintk_WHEN_width_with_string_SHOULD_pad_correctly() {
  char buffer[256];
  vprintk(buffer, "%10s", "test");
  TEST_ASSERT_EQUAL_STRING("      test", buffer);
}

void test_vprintk_WHEN_precision_with_int_SHOULD_pad_correctly() {
  char buffer[256];
  vprintk(buffer, "%.5d", 123);
  TEST_ASSERT_EQUAL_STRING("00123", buffer);
}

void test_vprintk_WHEN_width_and_precision_with_string_SHOULD_format_correctly() {
  char buffer[256];
  vprintk(buffer, "%10.5s", "hello world");
  TEST_ASSERT_EQUAL_STRING("     hello", buffer);
}

void test_vprintk_WHEN_asterisk_for_width_SHOULD_use_argument() {
  char buffer[256];
  vprintk(buffer, "%*d", 5, 123);
  TEST_ASSERT_EQUAL_STRING("  123", buffer);
}

void test_vprintk_WHEN_asterisk_for_precision_SHOULD_use_argument() {
  char buffer[256];
  vprintk(buffer, "%.*s", 3, "hello");
  TEST_ASSERT_EQUAL_STRING("hel", buffer);
}

void test_vprintk_WHEN_hash_with_hex_SHOULD_add_prefix() {
  char buffer[256];
  vprintk(buffer, "%#x", 255);
  TEST_ASSERT_EQUAL_STRING("0xff", buffer);
}

void test_vprintk_WHEN_percent_ld_SHOULD_print_long_decimal() {
    char buffer[256];
    vprintk(buffer, "%ld", 2147483648L);
    TEST_ASSERT_EQUAL_STRING("2147483648", buffer);

    vprintk(buffer, "%ld", -2147483649L);
    TEST_ASSERT_EQUAL_STRING("-2147483649", buffer);
}

void test_vprintk_WHEN_percent_lu_SHOULD_print_unsigned_long() {
    char buffer[256];
    vprintk(buffer, "%lu", 4294967295UL);
    TEST_ASSERT_EQUAL_STRING("4294967295", buffer);

    vprintk(buffer, "%lu", 0UL);
    TEST_ASSERT_EQUAL_STRING("0", buffer);
}

void test_vprintk_WHEN_percent_lx_SHOULD_print_long_hex() {
    char buffer[256];
    vprintk(buffer, "%lx", 0xABCDEF12UL);
    TEST_ASSERT_EQUAL_STRING("abcdef12", buffer);

    vprintk(buffer, "%lX", 0xABCDEF12UL);
    TEST_ASSERT_EQUAL_STRING("ABCDEF12", buffer);
}

void test_vprintk_WHEN_long_with_padding_SHOULD_format_correctly() {
    char buffer[256];
    vprintk(buffer, "%10ld", 123456L);
    TEST_ASSERT_EQUAL_STRING("   123456", buffer);

    vprintk(buffer, "%010lu", 123456UL);
    TEST_ASSERT_EQUAL_STRING("0000123456", buffer);

    vprintk(buffer, "%#10lx", 0xabcdUL);
    TEST_ASSERT_EQUAL_STRING("    0xabcd", buffer);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_vprintk_WHEN_no_percent_SHOULD_return_simple_text);

  RUN_TEST(test_vprintk_WHEN_percent_c_SHOULD_return_a_character);
  RUN_TEST(test_vprintk_WHEN_percent_5c_SHOULD_return_a_space_padded_character);
  RUN_TEST(test_vprintk_WHEN_percent_05c_SHOULD_return_a_space_padded_character);

  RUN_TEST(test_vprintk_WHEN_percent_d_SHOULD_return_a_positive_number);
  RUN_TEST(test_vprintk_WHEN_percent_d_SHOULD_return_a_negative_number);
  RUN_TEST(test_vprintk_WHEN_percent_5d_SHOULD_return_a_space_padded_number);
  RUN_TEST(test_vprintk_WHEN_percent_05d_SHOULD_return_a_zero_padded_number);

  RUN_TEST(test_vprintk_WHEN_percent_u_SHOULD_return_unsigned_integer);
  RUN_TEST(test_vprintk_WHEN_percent_x_SHOULD_return_lowercase_hex);
  RUN_TEST(test_vprintk_WHEN_percent_X_SHOULD_return_uppercase_hex);
  RUN_TEST(test_vprintk_WHEN_percent_s_SHOULD_return_string);
  RUN_TEST(test_vprintk_WHEN_percent_p_SHOULD_return_pointer_address);
  RUN_TEST(test_vprintk_WHEN_percent_percent_SHOULD_return_percent_sign);
  RUN_TEST(test_vprintk_WHEN_percent_minus_SHOULD_left_align);
  RUN_TEST(test_vprintk_WHEN_precision_SHOULD_limit_string_length);
  RUN_TEST(test_vprintk_WHEN_multiple_specifiers_SHOULD_format_correctly);
  RUN_TEST(test_vprintk_WHEN_width_and_precision_SHOULD_format_correctly);
  RUN_TEST(test_vprintk_WHEN_large_number_SHOULD_print_correctly);
  RUN_TEST(test_vprintk_WHEN_small_number_SHOULD_print_correctly);
  RUN_TEST(test_vprintk_WHEN_unsigned_long_SHOULD_print_correctly);
  RUN_TEST(test_vprintk_WHEN_long_long_SHOULD_print_correctly);
  RUN_TEST(test_vprintk_WHEN_multiple_args_SHOULD_print_correctly);
  RUN_TEST(test_vprintk_WHEN_width_with_string_SHOULD_pad_correctly);
  RUN_TEST(test_vprintk_WHEN_precision_with_int_SHOULD_pad_correctly);
  RUN_TEST(test_vprintk_WHEN_width_and_precision_with_string_SHOULD_format_correctly);
  RUN_TEST(test_vprintk_WHEN_asterisk_for_width_SHOULD_use_argument);
  RUN_TEST(test_vprintk_WHEN_asterisk_for_precision_SHOULD_use_argument);
  RUN_TEST(test_vprintk_WHEN_hash_with_hex_SHOULD_add_prefix);

  RUN_TEST(test_vprintk_WHEN_percent_ld_SHOULD_print_long_decimal);
  RUN_TEST(test_vprintk_WHEN_percent_lu_SHOULD_print_unsigned_long);
  RUN_TEST(test_vprintk_WHEN_percent_lx_SHOULD_print_long_hex);
  RUN_TEST(test_vprintk_WHEN_long_with_padding_SHOULD_format_correctly);

  return UNITY_END();
}