#include <ctype.h>

/**
 * Tests if a character is printable
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is printable (0x20-0x7E or 0x80-0xFE), 
 *         zero otherwise
 * @note Includes both ASCII and extended ASCII printable characters
 */
int isprint(int c) {
  return (c >= 0x20 && c <= 0x7e) || (c >= 0x80 && c < 0xff);
}

/**
 * Tests if a character is a whitespace character
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is a space, tab, newline, vertical tab,
 *         form feed, or carriage return; zero otherwise
 * @note Recognizes ' ', '\t', '\n', '\v', '\f', '\r' as whitespace
 */
int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/**
 * Tests if a character is a decimal digit
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is a digit (0-9), zero otherwise
 */
int isdigit(int c) {
  return c >= '0' && c <= '9';
}

int islower(int c) {
  return c >= 'a' && c <= 'z';
}

int isupper(int c) {
  return c >= 'A' && c <= 'Z';
}

/**
 * Tests if a character is an alphabetic character
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is a letter (a-z or A-Z), zero otherwise
 */
int isalpha(int c) {
  return islower(c) || isupper(c);
}

/**
 * Tests if a character is alphanumeric
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is a letter or digit, zero otherwise
 * @note Combines isalpha() and isdigit() checks
 */
int isalnum(int c) {
  return isalpha(c) || isdigit(c);
}

/**
 * Tests if a character is a hexadecimal digit
 * @param c The character to test (represented as an int)
 * @return Non-zero if the character is a hex digit (0-9, a-f, or A-F),
 *         zero otherwise
 */
int isxdigit(int c) {
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int iscntrl(int c) {
  return (c >= 0 && c < 0x20) || c == 0x7f;
}

int ispunct(int c) {
  return isprint(c) && !isalnum(c) && !isspace(c);
}

/**
 * Converts a character to lowercase
 * @param c The character to convert (represented as an int)
 * @return The lowercase equivalent if the character is an uppercase letter,
 *         otherwise returns the character unchanged
 */
int tolower(int c) {
  if(c >= 'A' && c <= 'Z') {
      c = c + ('a' - 'A');
  }
  return c;
}

/**
 * Converts a character to uppercase
 * @param c The character to convert (represented as an int)
 * @return The uppercase equivalent if the character is a lowercase letter,
 *         otherwise returns the character unchanged
 */
int toupper(int c) {
  if(c >= 'a' && c <= 'z') {
      c = c - ('a' - 'A');
  }
  return c;
}
