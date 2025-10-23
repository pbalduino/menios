#ifndef MENIOS_KERNEL

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

static void write_message(const char* message) {
  if(message == NULL) {
    return;
  }
  size_t len = 0;
  while(message[len] != '\0') {
    len++;
  }
  if(len > 0) {
    (void)write(STDERR_FILENO, message, len);
  }
}

void __menios_assert_fail(const char* expr,
                          const char* file,
                          int line,
                          const char* func) {
  char buffer[256];
  int written;

  if(func != NULL && func[0] != '\0') {
    written = snprintf(buffer,
                       sizeof(buffer),
                       "assertion failed: %s (%s:%d: %s)\n",
                       expr ? expr : "<expr>",
                       file ? file : "<file>",
                       line,
                       func);
  } else {
    written = snprintf(buffer,
                       sizeof(buffer),
                       "assertion failed: %s (%s:%d)\n",
                       expr ? expr : "<expr>",
                       file ? file : "<file>",
                       line);
  }

  if(written > 0) {
    if((size_t)written >= sizeof(buffer)) {
      buffer[sizeof(buffer) - 1] = '\0';
    }
    write_message(buffer);
  }

  _exit(134);
}

void abort(void) {
  write_message("abort()\n");
  _exit(134);
}

#endif
