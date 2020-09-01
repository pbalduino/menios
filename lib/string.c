#include <string.h>

size_t strlen(const char *s) {
  uint16_t len = 0;

  while(s[len++]);

  return len;
}

void swap(char* a, char* b) {
  char t = *b;
  *b = *a;
  *a = t;
}

void strrev(char str[], int length) {
  int start = 0;
  int end = length -1;
  while (start < end) {
    swap(&str[start], &str[end]);
    start++;
    end--;
  }
}
