#include <ctype.h>

int isprint(int c) {
  return (c >= 0x20 && c <= 0x7e) || (c >= 0x80 && c < 0xff);
}

int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int isdigit(int c) {
  return c >= '0' && c <= '9';
}

int isalpha(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c) {
  return isalpha(c) || isdigit(c);
}

int isxdigit(int c) {
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int tolower(int c) {
  if (c >= 'A' && c <= 'Z') {
      c = c + ('a' - 'A');
  }
  return c;
}

int toupper(int c) {
  if (c >= 'a' && c <= 'z') {
      c = c - ('a' - 'A');
  }
  return c;
}