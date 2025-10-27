#include <kernel/console.h>
#include <kernel/serial.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  LEN_DEFAULT = 0,
  LEN_LONG,
  LEN_LONGLONG
} length_modifier_t;

static inline void store_char(char* dest, size_t capacity, int pos, char value) {
  if(dest == NULL || capacity == 0) {
    return;
  }

  size_t limit = capacity - 1;
  if((size_t)pos <= limit) {
    dest[pos] = value;
  }
}

static int append_chars(char* dest, size_t capacity, int pos, char ch, int count) {
  while(count-- > 0) {
    store_char(dest, capacity, pos, ch);
    pos++;
  }
  return pos;
}

static int append_buffer(char* dest, size_t capacity, int pos, const char* buf, int len) {
  for(int i = 0; i < len; i++) {
    store_char(dest, capacity, pos, buf[i]);
    pos++;
  }
  return pos;
}

static void serial_print_hex(uintptr_t value) {
#ifdef MENIOS_KERNEL
  static const char digits[] = "0123456789abcdef";
  char hexbuf[sizeof(uintptr_t) * 2];
  for(int i = (int)sizeof(hexbuf) - 1; i >= 0; i--) {
    hexbuf[i] = digits[value & 0xfu];
    value >>= 4;
  }
  for(size_t i = 0; i < sizeof(hexbuf); i++) {
    serial_putchar(hexbuf[i]);
  }
#else
  (void)value;
#endif
}

static void report_invalid_string(uintptr_t value_addr) {
#ifdef MENIOS_KERNEL
  serial_puts("[vsnprintk] invalid string pointer 0x");
  serial_print_hex(value_addr);
  serial_puts(" ra0=0x");
  serial_print_hex((uintptr_t)__builtin_return_address(0));
#if defined(__has_builtin)
#  if __has_builtin(__builtin_return_address)
  serial_puts(" ra1=0x");
  serial_print_hex((uintptr_t)__builtin_return_address(1));
#  endif
#else
  serial_puts(" ra1=0x");
  serial_print_hex((uintptr_t)__builtin_return_address(1));
#endif
  serial_puts("\n");
#else
  (void)value_addr;
#endif
}

static int format_string(char* dest,
                         size_t capacity,
                         int pos,
                         const char* value,
                         bool left_align,
                         bool zero_pad,
                         int width,
                         bool precision_specified,
                         int precision) {
  if(value == NULL) {
    value = "(null)";
  } else {
    const uintptr_t kernel_floor = 0xffff800000000000ull;
    if((uintptr_t)value < kernel_floor) {
      report_invalid_string((uintptr_t)value);
      value = "(invalid-string)";
    }
  }

  int length = 0;
  while(value[length] != '\0' && (!precision_specified || length < precision)) {
    length++;
  }

  char pad_char = (zero_pad && !left_align) ? '0' : ' ';
  int padding = width > length ? width - length : 0;

  if(!left_align) {
    pos = append_chars(dest, capacity, pos, pad_char, padding);
  }

  pos = append_buffer(dest, capacity, pos, value, length);

  if(left_align) {
    pos = append_chars(dest, capacity, pos, ' ', padding);
  }

  return pos;
}

static int format_char(char* dest,
                       size_t capacity,
                       int pos,
                       char value,
                       bool left_align,
                       bool zero_pad,
                       int width) {
  char pad_char = (zero_pad && !left_align) ? '0' : ' ';
  int padding = width > 1 ? width - 1 : 0;

  if(!left_align) {
    pos = append_chars(dest, capacity, pos, pad_char, padding);
  }

  store_char(dest, capacity, pos, value);
  pos++;

  if(left_align) {
    pos = append_chars(dest, capacity, pos, ' ', padding);
  }

  return pos;
}

static int format_number(char* dest,
                         size_t capacity,
                         int pos,
                         uint64_t value,
                         int base,
                         bool uppercase,
                         bool left_align,
                         bool zero_pad,
                         bool precision_specified,
                         int precision,
                         int width,
                         const char* prefix,
                         int prefix_len) {
  char digits[65];
  int digit_len = 0;

  if(precision_specified && precision < 0) {
    precision_specified = false;
    precision = 0;
  }

  if(value == 0) {
    if(!(precision_specified && precision == 0)) {
      digits[digit_len++] = '0';
    }
  } else {
    while(value != 0) {
      uint64_t rem = value % (uint64_t)base;
      if(rem < 10) {
        digits[digit_len++] = (char)('0' + rem);
      } else {
        digits[digit_len++] = (char)((uppercase ? 'A' : 'a') + (rem - 10));
      }
      value /= (uint64_t)base;
    }
  }

  int zero_digits = 0;

  if(precision_specified) {
    if(precision > digit_len) {
      zero_digits = precision - digit_len;
    }
    zero_pad = false;
  }

  if(zero_pad && width > (prefix_len + digit_len)) {
    zero_digits = width - (prefix_len + digit_len);
  }

  if(zero_digits < 0) {
    zero_digits = 0;
  }

  int total_len = prefix_len + zero_digits + digit_len;
  int space_pad = width > total_len ? width - total_len : 0;

  if(!left_align) {
    pos = append_chars(dest, capacity, pos, ' ', space_pad);
  }

  if(prefix_len > 0) {
    pos = append_buffer(dest, capacity, pos, prefix, prefix_len);
  }

  pos = append_chars(dest, capacity, pos, '0', zero_digits);

  for(int i = digit_len - 1; i >= 0; i--) {
    store_char(dest, capacity, pos, digits[i]);
    pos++;
  }

  if(left_align) {
    pos = append_chars(dest, capacity, pos, ' ', space_pad);
  }

  return pos;
}

static int vsprintk_internal(char* str, size_t capacity, const char* format, va_list args) {
  int result_len = 0;

  for(int pos = 0; format[pos] != '\0';) {
    if(format[pos] != '%') {
      store_char(str, capacity, result_len, format[pos++]);
      result_len++;
      continue;
    }

    pos++;

    if(format[pos] == '%') {
      store_char(str, capacity, result_len, '%');
      result_len++;
      pos++;
      continue;
    }

    bool left_align = false;
    bool alternate_form = false;
    bool zero_pad = false;
    int width = 0;
    bool precision_specified = false;
    int precision = 0;
    length_modifier_t length = LEN_DEFAULT;

    bool parsing_flags = true;
    while(parsing_flags) {
      switch(format[pos]) {
      case '-':
        left_align = true;
        pos++;
        break;
      case '#':
        alternate_form = true;
        pos++;
        break;
      case '0':
        zero_pad = true;
        pos++;
        break;
      default:
        parsing_flags = false;
        break;
      }
    }

    if(format[pos] == '*') {
      width = va_arg(args, int);
      if(width < 0) {
        left_align = true;
        width = -width;
      }
      pos++;
    } else {
      while(isdigit((unsigned char)format[pos])) {
        width = width * 10 + (format[pos] - '0');
        pos++;
      }
    }

    if(format[pos] == '.') {
      pos++;
      precision_specified = true;
      if(format[pos] == '*') {
        precision = va_arg(args, int);
        if(precision < 0) {
          precision_specified = false;
          precision = 0;
        }
        pos++;
      } else {
        while(isdigit((unsigned char)format[pos])) {
          precision = precision * 10 + (format[pos] - '0');
          pos++;
        }
      }
    }

    if(format[pos] == 'l') {
      if(format[pos + 1] == 'l') {
        length = LEN_LONGLONG;
        pos += 2;
      } else {
        length = LEN_LONG;
        pos++;
      }
    }

    char specifier = format[pos];
    if(specifier == '\0') {
      break;
    }
    pos++;

    if(left_align) {
      zero_pad = false;
    }

    switch(specifier) {
    case 'c': {
      int value = va_arg(args, int);
      result_len = format_char(str, capacity, result_len, (char)value, left_align, zero_pad, width);
      break;
    }
    case 's': {
      const char* value = va_arg(args, const char*);
      result_len = format_string(str,
                                 capacity,
                                 result_len,
                                 value,
                                 left_align,
                                 zero_pad,
                                 width,
                                 precision_specified,
                                 precision);
      break;
    }
    case 'p': {
      uintptr_t value = (uintptr_t)va_arg(args, void*);
      char prefix_storage[3];
      prefix_storage[0] = '0';
      prefix_storage[1] = 'x';
      prefix_storage[2] = '\0';
      result_len = format_number(str,
                                 capacity,
                                 result_len,
                                 value,
                                 16,
                                 false,
                                 left_align,
                                 zero_pad,
                                 precision_specified,
                                 precision,
                                 width,
                                 prefix_storage,
                                 2);
      break;
    }
    case 'd':
    case 'i': {
      int64_t value;
      if(length == LEN_LONGLONG) {
        value = va_arg(args, long long);
      } else if(length == LEN_LONG) {
        value = va_arg(args, long);
      } else {
        value = va_arg(args, int);
      }

      bool negative = value < 0;
      uint64_t magnitude;

      if(negative) {
        magnitude = (uint64_t)(-(value + 1));
        magnitude += 1;
      } else {
        magnitude = (uint64_t)value;
      }

      char prefix_storage[2];
      int prefix_len = 0;
      if(negative) {
        prefix_storage[prefix_len++] = '-';
      }

      result_len = format_number(str,
                                 capacity,
                                 result_len,
                                 magnitude,
                                 10,
                                 false,
                                 left_align,
                                 zero_pad,
                                 precision_specified,
                                 precision,
                                 width,
                                 prefix_storage,
                                 prefix_len);
      break;
    }
    case 'z': {
      length = LEN_LONGLONG;
      pos++;
      if(format[pos] == '\0') {
        break;
      }
      specifier = format[pos++];
      switch(specifier) {
        case 'd':
        case 'i': {
          int64_t value = (int64_t)va_arg(args, long long);
          bool negative = value < 0;
          uint64_t magnitude;
          if(negative) {
            magnitude = (uint64_t)(-(value + 1));
            magnitude += 1;
          } else {
            magnitude = (uint64_t)value;
          }
          char prefix_storage[2];
          int prefix_len = 0;
          if(negative) {
            prefix_storage[prefix_len++] = '-';
          }
          result_len = format_number(str,
                                     capacity,
                                     result_len,
                                     magnitude,
                                     10,
                                     false,
                                     left_align,
                                     zero_pad,
                                     precision_specified,
                                     precision,
                                     width,
                                     prefix_storage,
                                     prefix_len);
          continue;
        }
        case 'u': {
          uint64_t value = (uint64_t)va_arg(args, size_t);
          result_len = format_number(str,
                                     capacity,
                                     result_len,
                                     value,
                                     10,
                                     false,
                                     left_align,
                                     zero_pad,
                                     precision_specified,
                                     precision,
                                     width,
                                     NULL,
                                     0);
          continue;
        }
        case 'x':
        case 'X': {
          bool uppercase = (specifier == 'X');
          uint64_t value = (uint64_t)va_arg(args, size_t);
          char prefix_storage[3];
          int prefix_len = 0;
          if(alternate_form && value != 0) {
            prefix_storage[prefix_len++] = '0';
            prefix_storage[prefix_len++] = uppercase ? 'X' : 'x';
          }
          result_len = format_number(str,
                                     capacity,
                                     result_len,
                                     value,
                                     16,
                                     uppercase,
                                     left_align,
                                     zero_pad,
                                     precision_specified,
                                     precision,
                                     width,
                                     prefix_len ? prefix_storage : NULL,
                                     prefix_len);
          continue;
        }
        case 'o': {
          uint64_t value = (uint64_t)va_arg(args, size_t);
          char prefix_storage[2];
          int prefix_len = 0;
          if(alternate_form && value != 0) {
            prefix_storage[prefix_len++] = '0';
          }
          result_len = format_number(str,
                                     capacity,
                                     result_len,
                                     value,
                                     8,
                                     false,
                                     left_align,
                                     zero_pad,
                                     precision_specified,
                                     precision,
                                     width,
                                     prefix_len ? prefix_storage : NULL,
                                     prefix_len);
          continue;
        }
        default:
          store_char(str, capacity, result_len++, '%');
          store_char(str, capacity, result_len++, 'z');
          store_char(str, capacity, result_len++, specifier);
          continue;
      }
    }
    case 'u': {
      uint64_t value;
      if(length == LEN_LONGLONG) {
        value = va_arg(args, unsigned long long);
      } else if(length == LEN_LONG) {
        value = va_arg(args, unsigned long);
      } else {
        value = va_arg(args, unsigned int);
      }

      result_len = format_number(str,
                                 capacity,
                                 result_len,
                                 value,
                                 10,
                                 false,
                                 left_align,
                                 zero_pad,
                                 precision_specified,
                                 precision,
                                 width,
                                 NULL,
                                 0);
      break;
    }
    case 'x':
    case 'X': {
      bool uppercase = specifier == 'X';
      uint64_t value;
      if(length == LEN_LONGLONG) {
        value = va_arg(args, unsigned long long);
      } else if(length == LEN_LONG) {
        value = va_arg(args, unsigned long);
      } else {
        value = va_arg(args, unsigned int);
      }

      char prefix_storage[3];
      int prefix_len = 0;
      if(alternate_form && value != 0) {
        prefix_storage[prefix_len++] = '0';
        prefix_storage[prefix_len++] = uppercase ? 'X' : 'x';
      }

      result_len = format_number(str,
                                 capacity,
                                 result_len,
                                 value,
                                 16,
                                 uppercase,
                                 left_align,
                                 zero_pad,
                                 precision_specified,
                                 precision,
                                 width,
                                 prefix_len ? prefix_storage : NULL,
                                 prefix_len);
      break;
    }
    case 'o': {
      uint64_t value;
      if(length == LEN_LONGLONG) {
        value = va_arg(args, unsigned long long);
      } else if(length == LEN_LONG) {
        value = va_arg(args, unsigned long);
      } else {
        value = va_arg(args, unsigned int);
      }

      char prefix_storage[2];
      int prefix_len = 0;
      if(alternate_form && value != 0) {
        prefix_storage[prefix_len++] = '0';
      }

      result_len = format_number(str,
                                 capacity,
                                 result_len,
                                 value,
                                 8,
                                 false,
                                 left_align,
                                 zero_pad,
                                 precision_specified,
                                 precision,
                                 width,
                                 prefix_len ? prefix_storage : NULL,
                                 prefix_len);
      break;
    }
    case '%': {
      result_len = format_char(str,
                               capacity,
                               result_len,
                               '%',
                               left_align,
                               zero_pad,
                               width > 1 ? width : 1);
      break;
    }
    default: {
      store_char(str, capacity, result_len++, specifier);
      break;
    }
    }
  }

  if(str != NULL && capacity > 0) {
    size_t limit = capacity - 1;
    size_t index = (size_t)result_len <= limit ? (size_t)result_len : limit;
    str[index] = '\0';
  }

  return result_len;
}

int vprintk(char *str, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsprintk_internal(str, SIZE_MAX, format, args);
  va_end(args);
  return len;
}

int vsprintk(char* str, const char* format, va_list args) {
  return vsprintk_internal(str, SIZE_MAX, format, args);
}

int vsnprintk(char* str, size_t capacity, const char* format, va_list args) {
  return vsprintk_internal(str, capacity, format, args);
}
