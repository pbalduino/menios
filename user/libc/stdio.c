#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <menios/syscall.h>
#include <menios/syscall_user.h>

enum {
  FILE_FLAG_CAN_READ   = 1u << 0,
  FILE_FLAG_CAN_WRITE  = 1u << 1,
  FILE_FLAG_APPEND     = 1u << 2,
  FILE_FLAG_EOF        = 1u << 3,
  FILE_FLAG_ERROR      = 1u << 4,
  FILE_FLAG_OWN_BUFFER = 1u << 5,
};

enum {
  FILE_LAST_OP_NONE  = 0,
  FILE_LAST_OP_READ  = 1,
  FILE_LAST_OP_WRITE = 2,
};

#define FILE_DEFAULT_BUFFER_SIZE 4096

static unsigned char stdin_buffer_storage[FILE_DEFAULT_BUFFER_SIZE];
static unsigned char stdout_buffer_storage[FILE_DEFAULT_BUFFER_SIZE];
static unsigned char stderr_buffer_storage[FILE_DEFAULT_BUFFER_SIZE];

static FILE stdin_stream = {
  .fd = STDIN_FILENO,
  .flags = FILE_FLAG_CAN_READ,
  .buffer = stdin_buffer_storage,
  .buffer_size = sizeof(stdin_buffer_storage),
  .buffer_pos = 0,
  .buffer_end = 0,
  .offset = 0,
  .error_number = 0,
  .last_op = FILE_LAST_OP_NONE,
};

static FILE stdout_stream = {
  .fd = STDOUT_FILENO,
  .flags = FILE_FLAG_CAN_WRITE,
  .buffer = stdout_buffer_storage,
  .buffer_size = sizeof(stdout_buffer_storage),
  .buffer_pos = 0,
  .buffer_end = 0,
  .offset = 0,
  .error_number = 0,
  .last_op = FILE_LAST_OP_NONE,
};

static FILE stderr_stream = {
  .fd = STDERR_FILENO,
  .flags = FILE_FLAG_CAN_WRITE,
  .buffer = stderr_buffer_storage,
  .buffer_size = sizeof(stderr_buffer_storage),
  .buffer_pos = 0,
  .buffer_end = 0,
  .offset = 0,
  .error_number = 0,
  .last_op = FILE_LAST_OP_NONE,
};

FILE* stdin = &stdin_stream;
FILE* stdout = &stdout_stream;
FILE* stderr = &stderr_stream;

static void stream_mark_error(FILE* stream, int err) {
  if(stream == NULL) {
    return;
  }
  stream->flags |= FILE_FLAG_ERROR;
  stream->error_number = err;
  errno = err;
}

static void stream_reset_buffer(FILE* stream) {
  if(stream == NULL) {
    return;
  }
  stream->buffer_pos = 0;
  stream->buffer_end = 0;
}

static bool stream_can_read(const FILE* stream) {
  return stream != NULL && (stream->flags & FILE_FLAG_CAN_READ) != 0;
}

static bool stream_can_write(const FILE* stream) {
  return stream != NULL && (stream->flags & FILE_FLAG_CAN_WRITE) != 0;
}

static int stream_flush_write(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(stream->last_op != FILE_LAST_OP_WRITE) {
    return 0;
  }

  size_t pending = stream->buffer_pos;
  size_t offset  = 0;

  while(offset < pending) {
    size_t remaining = pending - offset;
    ssize_t rc = write(stream->fd, stream->buffer + offset, remaining);
    if(rc < 0) {
      stream_mark_error(stream, errno);
      if(offset < pending) {
        memmove(stream->buffer, stream->buffer + offset, pending - offset);
        stream->buffer_pos = pending - offset;
      }
      return -1;
    }

    if(rc == 0) {
      stream_mark_error(stream, EIO);
      return -1;
    }

    offset += (size_t)rc;
    stream->offset += (off_t)rc;
  }

  stream->buffer_pos = 0;
  stream->buffer_end = 0;
  return 0;
}

static int stream_prepare_for_read(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(!stream_can_read(stream)) {
    stream_mark_error(stream, EBADF);
    return -1;
  }

  if(stream->last_op == FILE_LAST_OP_WRITE) {
    if(stream_flush_write(stream) < 0) {
      return -1;
    }
    stream->last_op = FILE_LAST_OP_NONE;
  }

  if(stream->last_op != FILE_LAST_OP_READ) {
    stream_reset_buffer(stream);
  }

  stream->last_op = FILE_LAST_OP_READ;
  stream->flags &= ~FILE_FLAG_EOF;
  return 0;
}

static int stream_prepare_for_write(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(!stream_can_write(stream)) {
    stream_mark_error(stream, EBADF);
    return -1;
  }

  if(stream->last_op == FILE_LAST_OP_READ) {
    size_t unread = 0;
    if(stream->buffer_end > stream->buffer_pos) {
      unread = stream->buffer_end - stream->buffer_pos;
    }
    if(unread > 0) {
      off_t rc = lseek(stream->fd, -(off_t)unread, SEEK_CUR);
      if(rc < 0) {
        stream_mark_error(stream, errno);
        return -1;
      }
      stream->offset = rc;
    }
    stream_reset_buffer(stream);
  }

  stream->last_op = FILE_LAST_OP_WRITE;
  stream->flags &= ~FILE_FLAG_EOF;
  return 0;
}

static bool parse_mode_string(const char* mode,
                              int* open_flags,
                              unsigned* file_flags) {
  if(mode == NULL || mode[0] == '\0') {
    return false;
  }

  char primary = mode[0];
  bool plus = false;

  for(const char* cursor = mode + 1; *cursor != '\0'; ++cursor) {
    if(*cursor == '+') {
      if(plus) {
        return false;
      }
      plus = true;
      continue;
    }

    if(*cursor == 'b' || *cursor == 't') {
      continue;
    }

    return false;
  }

  unsigned flags = 0;
  int oflags = 0;

  switch(primary) {
    case 'r':
      oflags = plus ? O_RDWR : O_RDONLY;
      flags = FILE_FLAG_CAN_READ | (plus ? FILE_FLAG_CAN_WRITE : 0u);
      break;
    case 'w':
      oflags = plus ? (O_RDWR | O_CREAT | O_TRUNC)
                    : (O_WRONLY | O_CREAT | O_TRUNC);
      flags = FILE_FLAG_CAN_WRITE | (plus ? FILE_FLAG_CAN_READ : 0u);
      break;
    case 'a':
      oflags = plus ? (O_RDWR | O_CREAT | O_APPEND)
                    : (O_WRONLY | O_CREAT | O_APPEND);
      flags = FILE_FLAG_CAN_WRITE | FILE_FLAG_APPEND
              | (plus ? FILE_FLAG_CAN_READ : 0u);
      break;
    default:
      return false;
  }

  *open_flags = oflags;
  *file_flags = flags;
  return true;
}

static bool multiply_will_overflow(size_t a, size_t b, size_t* out) {
  if(a == 0 || b == 0) {
    *out = 0;
    return true;
  }
  if(b > SIZE_MAX / a) {
    return false;
  }
  *out = a * b;
  return true;
}

FILE* fopen(const char* filename, const char* mode) {
  if(filename == NULL || mode == NULL) {
    errno = EINVAL;
    return NULL;
  }

  int open_flags = 0;
  unsigned file_flags = 0;
  if(!parse_mode_string(mode, &open_flags, &file_flags)) {
    errno = EINVAL;
    return NULL;
  }

  int fd = open(filename, open_flags, 0644);
  if(fd < 0) {
    return NULL;
  }

  FILE* stream = (FILE*)malloc(sizeof(FILE));
  if(stream == NULL) {
    int saved = errno;
    close(fd);
    errno = saved != 0 ? saved : ENOMEM;
    return NULL;
  }

  unsigned char* buffer = (unsigned char*)malloc(FILE_DEFAULT_BUFFER_SIZE);
  if(buffer == NULL) {
    int saved = errno;
    close(fd);
    free(stream);
    errno = saved != 0 ? saved : ENOMEM;
    return NULL;
  }

  stream->fd = fd;
  stream->flags = file_flags | FILE_FLAG_OWN_BUFFER;
  stream->buffer = buffer;
  stream->buffer_size = FILE_DEFAULT_BUFFER_SIZE;
  stream->buffer_pos = 0;
  stream->buffer_end = 0;
  stream->offset = 0;
  stream->error_number = 0;
  stream->last_op = FILE_LAST_OP_NONE;
  stream->flags &= ~(FILE_FLAG_EOF | FILE_FLAG_ERROR);

  int seek_origin = (file_flags & FILE_FLAG_APPEND) ? SEEK_END : SEEK_CUR;
  off_t position = lseek(fd, 0, seek_origin);
  if(position >= 0) {
    stream->offset = position;
  }

  return stream;
}

static int close_underlying_fd(FILE* stream) {
  if(close(stream->fd) < 0) {
    stream_mark_error(stream, errno);
    return -1;
  }
  stream->fd = -1;
  return 0;
}

int fclose(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return EOF;
  }

  if(stream == stdin || stream == stdout || stream == stderr) {
    errno = EBADF;
    return EOF;
  }

  int result = 0;
  if(stream->last_op == FILE_LAST_OP_WRITE && stream->buffer_pos > 0) {
    if(stream_flush_write(stream) < 0) {
      result = EOF;
    }
  }

  if(close_underlying_fd(stream) < 0) {
    result = EOF;
  }

  if((stream->flags & FILE_FLAG_OWN_BUFFER) && stream->buffer != NULL) {
    free(stream->buffer);
  }

  free(stream);
  return result;
}

int fflush(FILE* stream) {
  if(stream == NULL) {
    int rc_stdout = fflush(stdout);
    int rc_stderr = fflush(stderr);
    return (rc_stdout == 0 && rc_stderr == 0) ? 0 : EOF;
  }

  if(stream->last_op == FILE_LAST_OP_WRITE) {
    if(stream_flush_write(stream) < 0) {
      return EOF;
    }
    stream->last_op = FILE_LAST_OP_NONE;
  } else if(stream->last_op == FILE_LAST_OP_READ) {
    stream_reset_buffer(stream);
    stream->last_op = FILE_LAST_OP_NONE;
  }

  return 0;
}

long ftell(FILE* stream) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  off_t position = stream->offset;

  if(stream->last_op == FILE_LAST_OP_READ) {
    if(stream->buffer_end >= stream->buffer_pos) {
      position -= (off_t)(stream->buffer_end - stream->buffer_pos);
    }
  } else if(stream->last_op == FILE_LAST_OP_WRITE) {
    position += (off_t)stream->buffer_pos;
  }

  return (long)position;
}

int fseek(FILE* stream, long offset, int whence) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(stream->last_op == FILE_LAST_OP_WRITE) {
    if(stream_flush_write(stream) < 0) {
      return -1;
    }
  }

  if(stream->last_op == FILE_LAST_OP_READ && whence == SEEK_CUR) {
    if(stream->buffer_end >= stream->buffer_pos) {
      offset -= (long)(stream->buffer_end - stream->buffer_pos);
    }
  }

  off_t rc = lseek(stream->fd, (off_t)offset, whence);
  if(rc < 0) {
    stream_mark_error(stream, errno);
    return -1;
  }

  stream->offset = rc;
  stream_reset_buffer(stream);
  stream->last_op = FILE_LAST_OP_NONE;
  stream->flags &= ~(FILE_FLAG_EOF);
  return 0;
}

void rewind(FILE* stream) {
  if(stream == NULL) {
    return;
  }
  fseek(stream, 0, SEEK_SET);
  clearerr(stream);
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  if(ptr == NULL || stream == NULL) {
    errno = EINVAL;
    return 0;
  }

  if(size == 0 || nmemb == 0) {
    return 0;
  }

  size_t total = 0;
  if(!multiply_will_overflow(size, nmemb, &total)) {
    errno = EOVERFLOW;
    stream_mark_error(stream, EOVERFLOW);
    return 0;
  }

  if(stream_prepare_for_read(stream) < 0) {
    return 0;
  }

  unsigned char* out = (unsigned char*)ptr;
  size_t bytes_read = 0;

  while(bytes_read < total) {
    if(stream->buffer_pos < stream->buffer_end) {
      size_t available = stream->buffer_end - stream->buffer_pos;
      size_t need = total - bytes_read;
      size_t to_copy = (available < need) ? available : need;
      memcpy(out + bytes_read, stream->buffer + stream->buffer_pos, to_copy);
      stream->buffer_pos += to_copy;
      bytes_read += to_copy;
      continue;
    }

    ssize_t rc = read(stream->fd, stream->buffer, stream->buffer_size);
    if(rc < 0) {
      stream_mark_error(stream, errno);
      break;
    }
    if(rc == 0) {
      stream->flags |= FILE_FLAG_EOF;
      break;
    }

    stream->buffer_pos = 0;
    stream->buffer_end = (size_t)rc;
    stream->offset += (off_t)rc;
  }

  return bytes_read / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  if(ptr == NULL || stream == NULL) {
    errno = EINVAL;
    return 0;
  }

  if(size == 0 || nmemb == 0) {
    return 0;
  }

  size_t total = 0;
  if(!multiply_will_overflow(size, nmemb, &total)) {
    errno = EOVERFLOW;
    stream_mark_error(stream, EOVERFLOW);
    return 0;
  }

  if(stream_prepare_for_write(stream) < 0) {
    return 0;
  }

  const unsigned char* in = (const unsigned char*)ptr;
  size_t bytes_written = 0;

  while(bytes_written < total) {
    size_t space = stream->buffer_size - stream->buffer_pos;
    if(space == 0) {
      if(stream_flush_write(stream) < 0) {
        return bytes_written / size;
      }
      stream->last_op = FILE_LAST_OP_WRITE;
      space = stream->buffer_size;
    }

    size_t remaining = total - bytes_written;

    if(remaining >= stream->buffer_size && stream->buffer_pos == 0) {
      size_t chunk = remaining;
      ssize_t rc = write(stream->fd, in + bytes_written, chunk);
      if(rc < 0) {
        stream_mark_error(stream, errno);
        break;
      }
      if(rc == 0) {
        stream_mark_error(stream, EIO);
        break;
      }
      bytes_written += (size_t)rc;
      stream->offset += (off_t)rc;
      continue;
    }

    size_t to_copy = (remaining < space) ? remaining : space;
    memcpy(stream->buffer + stream->buffer_pos, in + bytes_written, to_copy);
    stream->buffer_pos += to_copy;
    bytes_written += to_copy;
  }

  return bytes_written / size;
}

int feof(FILE* stream) {
  if(stream == NULL) {
    return 0;
  }
  return (stream->flags & FILE_FLAG_EOF) ? 1 : 0;
}

int ferror(FILE* stream) {
  if(stream == NULL) {
    return 0;
  }
  return (stream->flags & FILE_FLAG_ERROR) ? 1 : 0;
}

void clearerr(FILE* stream) {
  if(stream == NULL) {
    return;
  }
  stream->flags &= ~(FILE_FLAG_ERROR | FILE_FLAG_EOF);
  stream->error_number = 0;
}

FILE* freopen(const char* filename, const char* mode, FILE* stream) {
  if(filename == NULL || mode == NULL || stream == NULL) {
    errno = EINVAL;
    return NULL;
  }

  if(stream_flush_write(stream) < 0) {
    return NULL;
  }

  stream_reset_buffer(stream);

  int open_flags = 0;
  unsigned file_flags = 0;
  if(!parse_mode_string(mode, &open_flags, &file_flags)) {
    errno = EINVAL;
    return NULL;
  }

  if(close_underlying_fd(stream) < 0) {
    return NULL;
  }

  int fd = open(filename, open_flags, 0644);
  if(fd < 0) {
    return NULL;
  }

  stream->fd = fd;
  unsigned preserved = stream->flags & FILE_FLAG_OWN_BUFFER;
  stream->flags = file_flags | preserved;
  stream_reset_buffer(stream);
  stream->offset = 0;
  stream->error_number = 0;
  stream->flags &= ~(FILE_FLAG_ERROR | FILE_FLAG_EOF);
  stream->last_op = FILE_LAST_OP_NONE;

  int origin = (file_flags & FILE_FLAG_APPEND) ? SEEK_END : SEEK_CUR;
  off_t pos = lseek(fd, 0, origin);
  if(pos >= 0) {
    stream->offset = pos;
  }

  return stream;
}

int remove(const char* path) {
  if(path == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(unlink(path) == 0) {
    return 0;
  }

  if(errno == EISDIR) {
    if(rmdir(path) == 0) {
      return 0;
    }
  }

  return -1;
}

int rename(const char* oldpath, const char* newpath) {
  if(oldpath == NULL || newpath == NULL) {
    errno = EINVAL;
    return -1;
  }

  long rc = __menios_syscall2(SYS_RENAME, (long)oldpath, (long)newpath);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }

  errno = 0;
  return 0;
}

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

static void format_number(fmt_buffer_t* buffer,
                          unsigned long magnitude,
                          bool is_negative,
                          unsigned base,
                          bool uppercase,
                          bool left_align,
                          bool zero_pad,
                          int field_width,
                          int precision,
                          bool precision_specified,
                          bool force_sign,
                          bool space_sign,
                          const char* prefix) {
  char digits_buf[64];
  size_t digit_index = 0;
  const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

  if(magnitude == 0) {
    digits_buf[digit_index++] = '0';
  } else {
    while(magnitude != 0 && digit_index < sizeof(digits_buf)) {
      digits_buf[digit_index++] = digits[magnitude % base];
      magnitude /= base;
    }
  }

  if(precision_specified && precision == 0 && digit_index == 1 && digits_buf[0] == '0') {
    digit_index = 0;
  }

  if(left_align) {
    zero_pad = false;
  }

  char sign = '\0';
  if(is_negative) {
    sign = '-';
  } else if(force_sign) {
    sign = '+';
  } else if(space_sign) {
    sign = ' ';
  }

  size_t prefix_len = prefix ? strlen(prefix) : 0;
  size_t sign_len = (sign != '\0') ? 1 : 0;

  size_t zero_count = 0;
  if(precision_specified) {
    if(precision > (int)digit_index) {
      zero_count = (size_t)(precision - (int)digit_index);
    }
    zero_pad = false;
  } else if(zero_pad && !left_align && field_width > 0) {
    int needed = field_width - (int)(sign_len + prefix_len + digit_index);
    if(needed > 0) {
      zero_count = (size_t)needed;
    }
  }

  size_t content_len = sign_len + prefix_len + zero_count + digit_index;
  size_t pad_len = 0;
  if(field_width > 0 && (size_t)field_width > content_len) {
    pad_len = (size_t)field_width - content_len;
  }

  if(!left_align) {
    for(size_t i = 0; i < pad_len; i++) {
      buffer_putc(buffer, ' ');
    }
  }

  if(sign != '\0') {
    buffer_putc(buffer, sign);
  }

  if(prefix_len > 0) {
    buffer_puts(buffer, prefix, prefix_len);
  }

  for(size_t i = 0; i < zero_count; i++) {
    buffer_putc(buffer, '0');
  }

  for(size_t i = 0; i < digit_index; i++) {
    buffer_putc(buffer, digits_buf[digit_index - 1 - i]);
  }

  if(left_align) {
    for(size_t i = 0; i < pad_len; i++) {
      buffer_putc(buffer, ' ');
    }
  }
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

    bool left_align = false;
    bool zero_pad_flag = false;
    bool force_sign = false;
    bool space_sign = false;
    bool alternate_form = false;

    bool parsing_flags = true;
    while(parsing_flags) {
      switch(*cursor) {
        case '-': left_align = true; cursor++; break;
        case '0': zero_pad_flag = true; cursor++; break;
        case '+': force_sign = true; cursor++; break;
        case ' ': space_sign = true; cursor++; break;
        case '#': alternate_form = true; cursor++; break;
        default: parsing_flags = false; break;
      }
    }

    int field_width = 0;
    while(*cursor >= '0' && *cursor <= '9') {
      field_width = field_width * 10 + (*cursor - '0');
      cursor++;
    }

    bool precision_specified = false;
    int precision = 0;
    if(*cursor == '.') {
      cursor++;
      precision_specified = true;
      while(*cursor >= '0' && *cursor <= '9') {
        precision = precision * 10 + (*cursor - '0');
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

    if(left_align) {
      zero_pad_flag = false;
    }

    char spec = *cursor++;
    switch(spec) {
      case 'd':
      case 'i': {
        long value = long_flag ? va_arg(args, long) : va_arg(args, int);
        bool negative = value < 0;
        unsigned long magnitude = negative ? (unsigned long)(-(value + 1)) + 1 : (unsigned long)value;
        format_number(&buffer,
                      magnitude,
                      negative,
                      10,
                      false,
                      left_align,
                      zero_pad_flag,
                      field_width,
                      precision,
                      precision_specified,
                      force_sign,
                      space_sign,
                      NULL);
        break;
      }
      case 'u': {
        unsigned long value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
        format_number(&buffer,
                      value,
                      false,
                      10,
                      false,
                      left_align,
                      zero_pad_flag,
                      field_width,
                      precision,
                      precision_specified,
                      false,
                      false,
                      NULL);
        break;
      }
      case 'x':
      case 'X': {
        unsigned long value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
        const char* prefix = NULL;
        if(alternate_form && value != 0) {
          prefix = (spec == 'X') ? "0X" : "0x";
        }
        format_number(&buffer,
                      value,
                      false,
                      16,
                      spec == 'X',
                      left_align,
                      zero_pad_flag,
                      field_width,
                      precision,
                      precision_specified,
                      false,
                      false,
                      prefix);
        break;
      }
      case 'p': {
        void* ptr = va_arg(args, void*);
        const char* prefix = "0x";
        format_number(&buffer,
                      (uintptr_t)ptr,
                      false,
                      16,
                      false,
                      left_align,
                      zero_pad_flag,
                      field_width,
                      precision,
                      precision_specified,
                      false,
                      false,
                      prefix);
        break;
      }
      case 's': {
        const char* str = va_arg(args, const char*);
        if(str == NULL) {
          str = "(null)";
        }
        size_t len = strlen(str);
        if(precision_specified && precision < (int)len) {
          len = (size_t)precision;
        }
        size_t pad_len = 0;
        if(field_width > 0 && (size_t)field_width > len) {
          pad_len = (size_t)field_width - len;
        }
        if(!left_align) {
          for(size_t i = 0; i < pad_len; i++) {
            buffer_putc(&buffer, ' ');
          }
        }
        buffer_puts(&buffer, str, len);
        if(left_align) {
          for(size_t i = 0; i < pad_len; i++) {
            buffer_putc(&buffer, ' ');
          }
        }
        break;
      }
      case 'c': {
        int ch = va_arg(args, int);
        size_t pad_len = 0;
        if(field_width > 1) {
          pad_len = (size_t)field_width - 1;
        }
        if(!left_align) {
          for(size_t i = 0; i < pad_len; i++) {
            buffer_putc(&buffer, ' ');
          }
        }
        buffer_putc(&buffer, (char)ch);
        if(left_align) {
          for(size_t i = 0; i < pad_len; i++) {
            buffer_putc(&buffer, ' ');
          }
        }
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

int vsprintf(char* str, const char* format, va_list args) {
  if(str == NULL) {
    errno = EINVAL;
    return -1;
  }
  return menios_vsnprintf(str, (size_t)-1, format, args);
}

int vsnprintf(char* str, size_t size, const char* format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int written = menios_vsnprintf(str, size, format, copy);
  va_end(copy);
  return written;
}

int snprintf(char* str, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int written = vsnprintf(str, size, format, args);
  va_end(args);
  return written;
}

static int menios_write_formatted(FILE* stream, const char* format, va_list args) {
  if(stream == NULL) {
    errno = EINVAL;
    return -1;
  }

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

  size_t written = fwrite(buffer, 1, length, stream);
  int result = (written == length) ? required : -1;
  free(buffer);
  return result;
}

int vfprintf(FILE* stream, const char* format, va_list args) {
  return menios_write_formatted(stream, format, args);
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
  size_t len = strlen(text);
  size_t written = fwrite(text, 1, len, stream);
  return (written == len) ? 0 : EOF;
}

int fputc(int ch, FILE* stream) {
  unsigned char byte = (unsigned char)ch;
  size_t written = fwrite(&byte, 1, 1, stream);
  if(written != 1) {
    return EOF;
  }
  return ch;
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
  if(fread(&ch, 1, 1, stdin) != 1) {
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

typedef struct {
  unsigned long long value;
  bool               negative;
} parsed_integer_t;

static void skip_input_whitespace(const char** cursor) {
  while(**cursor != '\0' && isspace((unsigned char)**cursor)) {
    (*cursor)++;
  }
}

static int digit_value(char ch) {
  if(ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if(ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 10;
  }
  if(ch >= 'A' && ch <= 'Z') {
    return ch - 'A' + 10;
  }
  return -1;
}

static int parse_integer(const char** cursor,
                         int width,
                         int base_hint,
                         bool allow_sign,
                         parsed_integer_t* out_value) {
  const char* p = *cursor;
  int         remaining = (width > 0) ? width : INT_MAX;

  if(remaining <= 0) {
    return 0;
  }

  bool negative = false;
  if(allow_sign && *p != '\0' && remaining > 0) {
    if(*p == '+' || *p == '-') {
      negative = (*p == '-');
      p++;
      remaining--;
    }
  }

  if(remaining <= 0) {
    return 0;
  }

  int base = base_hint;
  int prefix_len = 0;

  if((base == 0 || base == 16) && remaining >= 2 && p[0] == '0'
     && (p[1] == 'x' || p[1] == 'X')) {
    prefix_len = 2;
    if(base == 0) {
      base = 16;
    }
  }

  if(base == 0) {
    if(*p == '0') {
      base = 8;
    } else {
      base = 10;
    }
  }

  if(prefix_len > 0) {
    if(remaining < prefix_len) {
      return 0;
    }
    p += prefix_len;
    remaining -= prefix_len;
  }

  if(base == 16 && prefix_len == 0 && remaining >= 2 && p[0] == '0'
     && (p[1] == 'x' || p[1] == 'X')) {
    p += 2;
    remaining -= 2;
  }

  if(remaining <= 0) {
    return 0;
  }

  unsigned long long value = 0;
  int                digits = 0;

  while(remaining > 0 && *p != '\0') {
    int digit = digit_value(*p);
    if(digit < 0 || digit >= base) {
      break;
    }

    value = value * (unsigned long long)base + (unsigned long long)digit;
    p++;
    remaining--;
    digits++;
  }

  if(digits == 0) {
    return 0;
  }

  out_value->value = value;
  out_value->negative = negative;
  *cursor = p;
  return 1;
}

static int vsscanf_impl(const char* input, const char* format, va_list args) {
  const char* src = input;
  const char* fmt = format;
  int         assigned = 0;

  while(*fmt != '\0') {
    if(isspace((unsigned char)*fmt)) {
      while(isspace((unsigned char)*fmt)) {
        fmt++;
      }
      skip_input_whitespace(&src);
      continue;
    }

    if(*fmt != '%') {
      if(*src == '\0' || *src != *fmt) {
        return assigned;
      }
      src++;
      fmt++;
      continue;
    }

    fmt++;

    if(*fmt == '%') {
      if(*src == '%') {
        src++;
        fmt++;
        continue;
      }
      return assigned;
    }

    bool suppress_assignment = false;
    if(*fmt == '*') {
      suppress_assignment = true;
      fmt++;
    }

    int width = -1;
    if(isdigit((unsigned char)*fmt)) {
      width = 0;
      while(isdigit((unsigned char)*fmt)) {
        width = width * 10 + (*fmt - '0');
        fmt++;
      }
    }

    char length_mod = 0;
    if(*fmt == 'h' || *fmt == 'l') {
      length_mod = *fmt;
      fmt++;
      if(length_mod == 'l' && *fmt == 'l') {
        length_mod = 'q';
        fmt++;
      } else if(length_mod == 'h' && *fmt == 'h') {
        length_mod = 'H';
        fmt++;
      }
    }

    char conv = *fmt;
    if(conv == '\0') {
      return assigned;
    }
    fmt++;

    switch(conv) {
      case 'd':
      case 'i':
      case 'o':
      case 'u':
      case 'x':
      case 'X': {
        int base = 10;
        bool allow_sign = (conv == 'd' || conv == 'i');
        bool unsigned_conv = (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o');
        if(conv == 'o') {
          base = 8;
        } else if(conv == 'x' || conv == 'X') {
          base = 16;
        } else if(conv == 'i') {
          base = 0;
        }

        skip_input_whitespace(&src);

        parsed_integer_t parsed;
        const char*      before = src;
        if(parse_integer(&src, width, base, allow_sign, &parsed) == 0) {
          src = before;
          return assigned;
        }

        if(!suppress_assignment) {
          if(unsigned_conv) {
            unsigned long long uvalue = parsed.value;
            switch(length_mod) {
              case 'l': {
                unsigned long* out = va_arg(args, unsigned long*);
                *out = (unsigned long)uvalue;
                break;
              }
              case 'q': {
                unsigned long long* out = va_arg(args, unsigned long long*);
                *out = uvalue;
                break;
              }
              case 'h': {
                unsigned short* out = va_arg(args, unsigned short*);
                *out = (unsigned short)uvalue;
                break;
              }
              case 'H': {
                unsigned char* out = va_arg(args, unsigned char*);
                *out = (unsigned char)uvalue;
                break;
              }
              default: {
                unsigned int* out = va_arg(args, unsigned int*);
                *out = (unsigned int)uvalue;
                break;
              }
            }
          } else {
            long long sval = parsed.negative ? -(long long)parsed.value
                                             : (long long)parsed.value;
            switch(length_mod) {
              case 'l': {
                long* out = va_arg(args, long*);
                *out = (long)sval;
                break;
              }
              case 'q': {
                long long* out = va_arg(args, long long*);
                *out = sval;
                break;
              }
              case 'h': {
                short* out = va_arg(args, short*);
                *out = (short)sval;
                break;
              }
              case 'H': {
                signed char* out = va_arg(args, signed char*);
                *out = (signed char)sval;
                break;
              }
              default: {
                int* out = va_arg(args, int*);
                *out = (int)sval;
                break;
              }
            }
          }
          assigned++;
        }
        break;
      }

      case 's': {
        if(length_mod != 0) {
          return assigned;
        }

        skip_input_whitespace(&src);

        const char* start = src;
        int         max_chars = (width > 0) ? width : INT_MAX;
        int         taken = 0;

        while(*src != '\0' && !isspace((unsigned char)*src) && taken < max_chars) {
          src++;
          taken++;
        }

        if(taken == 0) {
          src = start;
          return assigned;
        }

        if(!suppress_assignment) {
          char* out = va_arg(args, char*);
          memcpy(out, start, (size_t)taken);
          out[taken] = '\0';
          assigned++;
        }
        break;
      }

      case 'c': {
        if(length_mod != 0) {
          return assigned;
        }

        int required = (width > 0) ? width : 1;
        const char* start = src;
        int copied = 0;

        if(!suppress_assignment) {
          char* out = va_arg(args, char*);
          while(copied < required && *src != '\0') {
            out[copied++] = *src++;
          }
          if(copied < required) {
            src = start;
            return assigned;
          }
          assigned++;
        } else {
          while(copied < required && *src != '\0') {
            src++;
            copied++;
          }
          if(copied < required) {
            src = start;
            return assigned;
          }
        }
        break;
      }

      case 'n': {
        ptrdiff_t consumed = src - input;

        if(!suppress_assignment) {
          switch(length_mod) {
            case 'l': {
              long* out = va_arg(args, long*);
              *out = (long)consumed;
              break;
            }
            case 'q': {
              long long* out = va_arg(args, long long*);
              *out = (long long)consumed;
              break;
            }
            case 'h': {
              short* out = va_arg(args, short*);
              *out = (short)consumed;
              break;
            }
            case 'H': {
              signed char* out = va_arg(args, signed char*);
              *out = (signed char)consumed;
              break;
            }
            default: {
              int* out = va_arg(args, int*);
              *out = (int)consumed;
              break;
            }
          }
        }
        break;
      }

      default:
        return assigned;
    }
  }

  return assigned;
}

int sscanf(const char* str, const char* format, ...) {
  if(str == NULL || format == NULL) {
    errno = EINVAL;
    return -1;
  }

  va_list args;
  va_start(args, format);
  int result = vsscanf(str, format, args);
  va_end(args);
  return result;
}

int vsscanf(const char* str, const char* format, va_list arg) {
  if(str == NULL || format == NULL) {
    errno = EINVAL;
    return -1;
  }

  va_list args_copy;
  va_copy(args_copy, arg);
  int result = vsscanf_impl(str, format, args_copy);
  va_end(args_copy);
  return result;
}

static int read_stream_into_buffer(FILE* stream,
                                   char** out_buffer,
                                   size_t* out_length) {
  size_t capacity = 256;
  char* buffer = (char*)malloc(capacity);
  if(buffer == NULL) {
    errno = ENOMEM;
    return -1;
  }

  size_t length = 0;
  bool   saw_newline = false;

  while(!saw_newline) {
    if(length + 1 >= capacity) {
      size_t new_capacity = capacity * 2;
      char* new_buffer = (char*)realloc(buffer, new_capacity);
      if(new_buffer == NULL) {
        free(buffer);
        errno = ENOMEM;
        return -1;
      }
      buffer = new_buffer;
      capacity = new_capacity;
    }

    size_t chunk = capacity - length - 1;
    size_t rc = fread(buffer + length, 1, chunk, stream);
    if(rc == 0) {
      if(ferror(stream)) {
        free(buffer);
        return -1;
      }
      break;
    }

    length += (size_t)rc;
    buffer[length] = '\0';

    if(memchr(buffer + (length - (size_t)rc), '\n', (size_t)rc) != NULL) {
      saw_newline = true;
    }
  }

  buffer[length] = '\0';

  if(length == 0) {
    free(buffer);
    return 1;
  }

  *out_buffer = buffer;
  *out_length = length;
  return 0;
}

int vscanf(const char* format, va_list arg) {
  if(format == NULL) {
    errno = EINVAL;
    return -1;
  }

  char* buffer = NULL;
  size_t length = 0;
  int rc = read_stream_into_buffer(stdin, &buffer, &length);
  if(rc == 1) {
    return EOF;
  }
  if(rc < 0) {
    return -1;
  }

  va_list args_copy;
  va_copy(args_copy, arg);
  int result = vsscanf_impl(buffer, format, args_copy);
  va_end(args_copy);
  free(buffer);
  return result;
}

int scanf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vscanf(format, args);
  va_end(args);
  return result;
}

int vfscanf(FILE* stream, const char* format, va_list arg) {
  if(stream == NULL || format == NULL) {
    errno = EINVAL;
    return -1;
  }

  char* buffer = NULL;
  size_t length = 0;
  int rc = read_stream_into_buffer(stream, &buffer, &length);
  if(rc == 1) {
    return EOF;
  }
  if(rc < 0) {
    return -1;
  }

  va_list args_copy;
  va_copy(args_copy, arg);
  int result = vsscanf_impl(buffer, format, args_copy);
  va_end(args_copy);
  free(buffer);
  return result;
}

int fscanf(FILE* stream, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vfscanf(stream, format, args);
  va_end(args);
  return result;
}
