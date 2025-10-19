#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/condvar.h>
#include <kernel/file.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/serial.h>

#define PIPE_BUFFER_SIZE 4096u

typedef struct pipe_shared_t {
  kmutex_t   lock;
  kcondvar_t readable;
  kcondvar_t writable;
  uint8_t    buffer[PIPE_BUFFER_SIZE];
  size_t     head;
  size_t     tail;
  size_t     count;
  uint32_t   readers;
  uint32_t   writers;
} pipe_shared_t;

typedef struct pipe_endpoint_t {
  pipe_shared_t* shared;
  bool           is_reader;
} pipe_endpoint_t;

static const file_ops_t pipe_file_ops;

static void pipe_shared_init(pipe_shared_t* shared) {
  kmutex_init(&shared->lock);
  kcondvar_init(&shared->readable);
  kcondvar_init(&shared->writable);
  shared->head = 0;
  shared->tail = 0;
  shared->count = 0;
  shared->readers = 1;
  shared->writers = 1;
}

static void pipe_shared_write(pipe_shared_t* shared, const uint8_t* data, size_t length) {
  for(size_t idx = 0; idx < length; idx++) {
    shared->buffer[shared->tail] = data[idx];
    shared->tail = (shared->tail + 1) % PIPE_BUFFER_SIZE;
  }
  shared->count += length;
}

static size_t pipe_shared_read(pipe_shared_t* shared, uint8_t* buffer, size_t length) {
  size_t consumed = 0;
  while(consumed < length && shared->count > 0) {
    buffer[consumed++] = shared->buffer[shared->head];
    shared->head = (shared->head + 1) % PIPE_BUFFER_SIZE;
    shared->count--;
  }
  return consumed;
}

static int64_t pipe_read_impl(file_t* file, void* buffer, size_t length) {
  if(length == 0) {
    return 0;
  }

  pipe_endpoint_t* endpoint = (pipe_endpoint_t*)file->private_data;
  if(endpoint == NULL || !endpoint->is_reader) {
    return -EBADF;
  }

  pipe_shared_t* shared = endpoint->shared;
  uint8_t* out = (uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&shared->lock);
  for(;;) {
    while(shared->count == 0) {
      if(shared->writers == 0) {
        kmutex_unlock(&shared->lock);
        return (int64_t)total;
      }
      serial_printf("pipe_read: waiting (writers=%u)\n", shared->writers);
      kcondvar_wait(&shared->readable, &shared->lock);
    }

    size_t available = shared->count;
    size_t remaining = length - total;
    size_t chunk = available < remaining ? available : remaining;
    chunk = pipe_shared_read(shared, out + total, chunk);
    total += chunk;
    kcondvar_signal(&shared->writable);

    if(total > 0) {
      break;
    }
  }
  kmutex_unlock(&shared->lock);

  return (int64_t)total;
}

static int64_t pipe_write_impl(file_t* file, const void* buffer, size_t length) {
  if(length == 0) {
    return 0;
  }

  pipe_endpoint_t* endpoint = (pipe_endpoint_t*)file->private_data;
  if(endpoint == NULL || endpoint->is_reader) {
    return -EBADF;
  }

  pipe_shared_t* shared = endpoint->shared;
  const uint8_t* data = (const uint8_t*)buffer;
  size_t total = 0;

  kmutex_lock(&shared->lock);
  while(total < length) {
    if(shared->readers == 0) {
      kmutex_unlock(&shared->lock);
      return (total > 0) ? (int64_t)total : -EPIPE;
    }

    while(shared->count == PIPE_BUFFER_SIZE) {
      if(shared->readers == 0) {
        kmutex_unlock(&shared->lock);
        return -EPIPE;
      }
      kcondvar_wait(&shared->writable, &shared->lock);
    }

    size_t space = PIPE_BUFFER_SIZE - shared->count;
    size_t remaining = length - total;
    size_t chunk = space < remaining ? space : remaining;
    pipe_shared_write(shared, data + total, chunk);
    total += chunk;
    kcondvar_signal(&shared->readable);

    if(chunk == 0) {
      break;
    }
  }
  kmutex_unlock(&shared->lock);

  return (int64_t)total;
}

static int pipe_close_impl(file_t* file) {
  pipe_endpoint_t* endpoint = (pipe_endpoint_t*)file->private_data;
  if(endpoint == NULL) {
    return -EINVAL;
  }

  pipe_shared_t* shared = endpoint->shared;
  bool cleanup = false;

  kmutex_lock(&shared->lock);
  if(endpoint->is_reader) {
    if(shared->readers > 0) {
      shared->readers--;
    }
    if(shared->readers == 0) {
      serial_printf("pipe_close: readers=0 writers=%u\n", shared->writers);
      kcondvar_broadcast(&shared->writable);
    }
  } else {
    if(shared->writers > 0) {
      shared->writers--;
    }
    if(shared->writers == 0) {
      serial_printf("pipe_close: writers=0 readers=%u\n", shared->readers);
      kcondvar_broadcast(&shared->readable);
    }
  }
  cleanup = (shared->readers == 0 && shared->writers == 0);
  kmutex_unlock(&shared->lock);

  kfree(endpoint);
  if(cleanup) {
    kfree(shared);
  }

  return 0;
}

static const file_ops_t pipe_file_ops = {
  .read = pipe_read_impl,
  .write = pipe_write_impl,
  .close = pipe_close_impl,
  .seek = NULL,
  .ioctl = NULL,
  .mmap = NULL,
};

int pipe_create(file_t** read_end, file_t** write_end) {
  if(read_end == NULL || write_end == NULL) {
    return -EINVAL;
  }

  pipe_shared_t* shared = kmalloc(sizeof(pipe_shared_t));
  if(shared == NULL) {
    return -ENOMEM;
  }
  pipe_shared_init(shared);

  pipe_endpoint_t* reader = kmalloc(sizeof(pipe_endpoint_t));
  if(reader == NULL) {
    kfree(shared);
    return -ENOMEM;
  }

  pipe_endpoint_t* writer = kmalloc(sizeof(pipe_endpoint_t));
  if(writer == NULL) {
    kfree(reader);
    kfree(shared);
    return -ENOMEM;
  }

  reader->shared = shared;
  reader->is_reader = true;
  writer->shared = shared;
  writer->is_reader = false;

  file_t* read_file = file_create(&pipe_file_ops, reader, FILE_MODE_READ);
  if(read_file == NULL) {
    kfree(writer);
    kfree(reader);
    kfree(shared);
    return -ENOMEM;
  }

  file_t* write_file = file_create(&pipe_file_ops, writer, FILE_MODE_WRITE);
  if(write_file == NULL) {
    file_unref(read_file);
    kfree(writer);
    kfree(shared);
    return -ENOMEM;
  }

  *read_end = read_file;
  *write_end = write_file;
  return 0;
}
