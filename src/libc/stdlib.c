#include <stdlib.h>
#include <string.h>

#include <kernel/serial.h>

char* utoa(uint32_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }
  // Process individual digits
  while (num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if (isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

char* itoa(int32_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // In standard itoa(), negative numbers are handled only with
  // base 10. Otherwise numbers are considered unsigned.
  if (num < 0) {
    isNegative = true;
    num = -num;
  }

  // Process individual digits
  while (num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem - 10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if (isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

char* lutoa(uint64_t num, char* str, int32_t base) {
  int i = 0;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Process individual digits
  while (num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}

char* ltoa(int64_t num, char* str, int32_t base) {
  int i = 0;
  bool isNegative = false;

  /* Handle 0 explicitely, otherwise empty string is printed for 0 */
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // In standard itoa(), negative numbers are handled only with
  // base 10. Otherwise numbers are considered unsigned.
  if (num < 0) {
    isNegative = true;
    num = -num;
  }

  // Process individual digits
  while (num != 0) {
    int rem = num % base;
    str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    num = num / base;
  }

  // If number is negative, append '-'
  if (isNegative) {
    str[i++] = '-';
   }

  str[i] = '\0'; // Append string terminator

  // Reverse the string
  strrev(str, i);

  return str;
}
