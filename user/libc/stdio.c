#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>

static FILE stdin_stream = { .reserved = STDIN_FILENO };
static FILE stdout_stream = { .reserved = STDOUT_FILENO };
static FILE stderr_stream = { .reserved = STDERR_FILENO };

FILE* stdin = &stdin_stream;
FILE* stdout = &stdout_stream;
FILE* stderr = &stderr_stream;

typedef struct {
  char*  start;
  size_t capacity;
  size_t index;
  size_t written;
} fmt_buffer_t;

static void buffer_putc(fmt_buffer_t* buffer, char ch) {
  if(buffer->start != NULL && buffer->capacity > 0 && buffer->index < buffer->capacity - 1) {
    buffer->start[buffer->index] = ch;
  }
  buffer->index++;
  buffer->written++;
}

static void buffer_puts(fmt_buffer_t* buffer, const char* text, size_t length) {
  for(size_t i = 0; i < length; i++) {
    buffer_putc(buffer, text[i]);
  }
}

static void format_unsigned(fmt_buffer_t* buffer, unsigned long value, unsigned base, bool uppercase) {
  const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  char tmp[32];
  size_t index = 0;

  do {
    tmp[index++] = digits[value % base];
    value /= base;
  } while(value != 0 && index < sizeof(tmp));

  while(index-- > 0) {
    buffer_putc(buffer, tmp[index]);
  }
}

static void format_signed(fmt_buffer_t* buffer, long value) {
  unsigned long magnitude;
  if(value < 0) {
    buffer_putc(buffer, '-');
    magnitude = (unsigned long)(-(value + 1)) + 1;
  } else {
    magnitude = (unsigned long)value;
  }
  format_unsigned(buffer, magnitude, 10, false);
}

static int menios_vsnprintf(char* dest, size_t size, const char* format, va_list args) {
  fmt_buffer_t buffer = {
    .start = dest,
    .capacity = size,
    .index = 0,
    .written = 0,
  };

  const char* cursor = format;
  while(*cursor != '\0') {
    if(*cursor != '%') {
      buffer_putc(&buffer, *cursor++);
      continue;
    }

    cursor++;

    while(*cursor == '0' || *cursor == '-' || *cursor == '+' ||
          *cursor == ' ' || *cursor == '#') {
      cursor++;
    }

    while(*cursor >= '0' && *cursor <= '9') {
      cursor++;
    }

    if(*cursor == '.') {
      cursor++;
      while(*cursor >= '0' && *cursor <= '9') {
        cursor++;
      }
    }

    bool long_flag = false;
    if(*cursor == 'l') {
      long_flag = true;
      cursor++;
      if(*cursor == 'l') {
        cursor++;
      }
    }

    char spec = *cursor++;
    switch(spec) {
      case 'd':
      case 'i': {
        long value = long_flag ? va_arg(args, long) : va_arg(args, int);
        format_signed(&buffer, value);
        break;
      }
      case 'u': {
        unsigned long value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
        format_unsigned(&buffer, value, 10, false);
        break;
      }
      case 'x':
      case 'X': {
        unsigned long value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
        format_unsigned(&buffer, value, 16, spec == 'X');
        break;
      }
      case 'p': {
        void* ptr = va_arg(args, void*);
        buffer_puts(&buffer, "0x", 2);
        format_unsigned(&buffer, (uintptr_t)ptr, 16, false);
        break;
      }
      case 's': {
        const char* str = va_arg(args, const char*);
        if(str == NULL) {
          str = "(null)";
        }
        buffer_puts(&buffer, str, strlen(str));
        break;
      }
      case 'c': {
        int ch = va_arg(args, int);
        buffer_putc(&buffer, (char)ch);
        break;
      }
      case '%':
        buffer_putc(&buffer, '%');
        break;
      default:
        buffer_putc(&buffer, '%');
        buffer_putc(&buffer, spec);
        break;
    }
  }

  if(buffer.start != NULL && buffer.capacity > 0) {
    size_t terminator_index = (buffer.index < buffer.capacity) ? buffer.index : buffer.capacity - 1;
    buffer.start[terminator_index] = '\0';
  }

  return (int)buffer.written;
}

static int menios_stream_fd(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }
  return stream->reserved;
}

int svprintf(char* str, const char* format, va_list arg) {
  va_list measure;
  va_copy(measure, arg);
  int needed = menios_vsnprintf(NULL, 0, format, measure);
  va_end(measure);

  if(needed < 0) {
    return needed;
  }

  size_t total = (size_t)needed + 1;
  va_list render;
  va_copy(render, arg);
  menios_vsnprintf(str, total, format, render);
  va_end(render);
  return needed;
}

int sprintf(char* str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int written = svprintf(str, format, args);
  va_end(args);
  return written;
}

static int menios_write_formatted(int fd, const char* format, va_list args) {
  va_list measure;
  va_copy(measure, args);
  int required = menios_vsnprintf(NULL, 0, format, measure);
  va_end(measure);

  if(required < 0) {
    return -1;
  }

  size_t length = (size_t)required;
  char* buffer = (char*)malloc(length + 1);
  if(buffer == NULL) {
    errno = ENOMEM;
    return -1;
  }

  va_list render;
  va_copy(render, args);
  menios_vsnprintf(buffer, length + 1, format, render);
  va_end(render);

  ssize_t rc = write(fd, buffer, length);
  int result = (rc < 0) ? -1 : required;
  free(buffer);
  return result;
}

int vfprintf(FILE* stream, const char* format, va_list args) {
  int fd = menios_stream_fd(stream);
  if(fd < 0) {
    return -1;
  }
  return menios_write_formatted(fd, format, args);
}

int fprintf(FILE* stream, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int rc = vfprintf(stream, format, args);
  va_end(args);
  return rc;
}

int vprintf(const char* format, va_list args) {
  return vfprintf(stdout, format, args);
}

int printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int rc = vfprintf(stdout, format, args);
  va_end(args);
  return rc;
}

int fvprintf(FILE* stream, const char* format, va_list arg) {
  return vfprintf(stream, format, arg);
}

int fputs(const char* text, FILE* stream) {
  int fd = menios_stream_fd(stream);
  if(fd < 0) {
    return EOF;
  }
  size_t len = strlen(text);
  ssize_t rc = write(fd, text, len);
  return (rc < 0) ? EOF : 0;
}

int fputc(int ch, FILE* stream) {
  int fd = menios_stream_fd(stream);
  if(fd < 0) {
    return EOF;
  }
  unsigned char byte = (unsigned char)ch;
  ssize_t rc = write(fd, &byte, 1);
  return (rc < 0) ? EOF : ch;
}

int putchar(int ch) {
  return fputc(ch, stdout);
}

int puts(const char* str) {
  if(fputs(str, stdout) == EOF) {
    return EOF;
  }
  return fputc('\n', stdout);
}

int getchar(void) {
  unsigned char ch;
  ssize_t rc = read(STDIN_FILENO, &ch, 1);
  if(rc <= 0) {
    return EOF;
  }
  return ch;
}

char* gets(char* str) {
  (void)str;
  errno = ENOSYS;
  return NULL;
}

static const struct {
  int code;
  const char* text;
} errno_table[] = {
  { EINVAL, "Invalid argument" },
  { ENOMEM, "Out of memory" },
  { ENOENT, "No such file or directory" },
  { EIO,    "Input/output error" },
  { EPERM,  "Operation not permitted" },
  { EACCES, "Permission denied" },
};

static const char* menios_strerror(int err) {
  for(size_t i = 0; i < sizeof(errno_table) / sizeof(errno_table[0]); i++) {
    if(errno_table[i].code == err) {
      return errno_table[i].text;
    }
  }
  return NULL;
}

void perror(const char* s) {
  const char* message = menios_strerror(errno);
  char fallback[64];

  if(message == NULL) {
    sprintf(fallback, "Unknown error %d", errno);
    message = fallback;
  }

  if(s != NULL && *s != '\0') {
    char buffer[256];
    sprintf(buffer, "%s: %s\n", s, message);
    write(STDERR_FILENO, buffer, strlen(buffer));
  } else {
    char buffer[256];
    sprintf(buffer, "%s\n", message);
    write(STDERR_FILENO, buffer, strlen(buffer));
  }
}
