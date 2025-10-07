#include <stdlib.h>
#include <string.h>

#ifdef MENIOS_KERNEL
#include <kernel/serial.h>
#endif

/**
 * Converts an unsigned 32-bit integer to a string representation
 * @param num The unsigned integer to convert
 * @param str Pointer to the buffer that will hold the resulting string
 * @param base The base for conversion (2 to 36)
 * @return Pointer to the resulting string
 * @note The buffer must be large enough to hold the resulting string
 * @note For bases > 10, digits after 9 are represented by lowercase letters a-z
 */
char* utoa(uint32_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if(num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }
  // Process individual digits
  while(num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if(isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

/**
 * Converts a signed 32-bit integer to a string representation
 * @param num The signed integer to convert
 * @param str Pointer to the buffer that will hold the resulting string
 * @param base The base for conversion (2 to 36)
 * @return Pointer to the resulting string
 * @note The buffer must be large enough to hold the resulting string
 * @note Negative numbers are handled only with base 10
 * @note For bases > 10, digits after 9 are represented by lowercase letters a-z
 */
char* itoa(int32_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if(num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // In standard itoa(), negative numbers are handled only with
  // base 10. Otherwise numbers are considered unsigned.
  if(num < 0) {
    isNegative = true;
    num = -num;
  }

  // Process individual digits
  while(num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem - 10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if(isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

/**
 * Converts an unsigned 64-bit integer to a string representation
 * @param num The unsigned long integer to convert
 * @param str Pointer to the buffer that will hold the resulting string
 * @param base The base for conversion (2 to 36)
 * @return Pointer to the resulting string
 * @note The buffer must be large enough to hold the resulting string
 * @note For bases > 10, digits after 9 are represented by lowercase letters a-z
 */
char* lutoa(uint64_t num, char* str, int32_t base) {
  int i = 0;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if(num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Process individual digits
  while(num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

/**
 * Converts an unsigned 64-bit integer to a string representation in the specified base
 *
 * @param num   The unsigned 64-bit integer to convert
 * @param str   The character array where the result will be stored
 * @param base  The base for the conversion (e.g. 2 for binary, 10 for decimal, 16 for hex)
 *
 * @return      Pointer to the converted string
 *
 * The function handles special case for 0, converts each digit, and reverses the
 * resulting string. For bases > 10, digits 10-35 are represented as 'A'-'Z'.
 * The caller must ensure that str has enough space to store the result.
 */
char* lutoca(uint64_t num, char* str, int32_t base) {
  int i = 0;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if(num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Process individual digits
  while(num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'A' : rem + '0';
    num = num / base;
  }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

/**
 * Converts a signed 64-bit integer to a string representation
 * @param num The signed long integer to convert
 * @param str Pointer to the buffer that will hold the resulting string
 * @param base The base for conversion (2 to 36)
 * @return Pointer to the resulting string
 * @note The buffer must be large enough to hold the resulting string
 * @note Negative numbers are handled only with base 10
 * @note For bases > 10, digits after 9 are represented by lowercase letters a-z
 */
char* ltoa(int64_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if(num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // In standard itoa(), negative numbers are handled only with
  // base 10. Otherwise numbers are considered unsigned.
  if(num < 0) {
    isNegative = true;
    num = -num;
  }

  // Process individual digits
  while(num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if(isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}
