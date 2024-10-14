#include <kernel/console.h>

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

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_vprintk_WHEN_no_percent_SHOULD_return_simple_text);
  RUN_TEST(test_vprintk_WHEN_percent_c_SHOULD_return_a_character);
  RUN_TEST(test_vprintk_WHEN_percent_d_SHOULD_return_a_positive_number);
  RUN_TEST(test_vprintk_WHEN_percent_d_SHOULD_return_a_negative_number);

  return UNITY_END();
}