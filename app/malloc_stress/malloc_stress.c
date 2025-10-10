#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ITERATIONS  2000u
#define DEFAULT_SLOTS        128u
#define MAX_ALLOC_SIZE     16384u

typedef struct slot_entry {
  uint8_t* ptr;
  size_t   size;
  uint8_t  pattern;
} slot_entry_t;

static uint32_t lcg_next(uint32_t* state) {
  *state = (*state * 1664525u) + 1013904223u;
  return *state;
}

static void fill_pattern(uint8_t* ptr, size_t size, uint8_t pattern) {
  memset(ptr, (int)pattern, size);
}

static int verify_pattern(const slot_entry_t* entry) {
  for(size_t i = 0; i < entry->size; ++i) {
    if(entry->ptr[i] != entry->pattern) {
      fprintf(stderr,
              "malloc-stress: pattern mismatch at byte %zu (expected %u, got %u)\n",
              i,
              (unsigned)entry->pattern,
              (unsigned)entry->ptr[i]);
      return -1;
    }
  }
  return 0;
}

static unsigned parse_iterations(int argc, char** argv) {
  if(argc < 2 || argv[1] == NULL) {
    return DEFAULT_ITERATIONS;
  }

  char* end = NULL;
  long value = strtol(argv[1], &end, 10);
  if(end == argv[1] || *end != '\0') {
    fprintf(stderr, "malloc-stress: invalid iteration count '%s'\n", argv[1]);
    return DEFAULT_ITERATIONS;
  }
  if(value <= 0 || value > 1000000l) {
    fprintf(stderr, "malloc-stress: iteration count out of range (%ld)\n", value);
    return DEFAULT_ITERATIONS;
  }
  return (unsigned)value;
}

int main(int argc, char** argv) {
  unsigned iterations = parse_iterations(argc, argv);
  slot_entry_t slots[DEFAULT_SLOTS];
  memset(slots, 0, sizeof(slots));
  uint32_t state = 0xC0FFEEu;
  unsigned reallocations = 0;
  unsigned allocations = 0;

  printf("malloc-stress: running %u iterations across %u slots\n",
         iterations,
         (unsigned)DEFAULT_SLOTS);

  for(unsigned iter = 0; iter < iterations; ++iter) {
    uint32_t r = lcg_next(&state);
    size_t index = r % DEFAULT_SLOTS;
    slot_entry_t* entry = &slots[index];

    if(entry->ptr != NULL) {
      if(verify_pattern(entry) != 0) {
        goto failure;
      }

      if((r & 0x7u) == 0u) {
        size_t new_size = (lcg_next(&state) % MAX_ALLOC_SIZE) + 1u;
        uint8_t new_pattern = (uint8_t)new_size;
        errno = 0;
        uint8_t* grown = realloc(entry->ptr, new_size);
        if(grown == NULL) {
          fprintf(stderr, "malloc-stress: realloc failure at iter %u (errno=%d)\n", iter, errno);
          goto failure;
        }
        size_t min_copy = (entry->size < new_size) ? entry->size : new_size;
        for(size_t i = 0; i < min_copy; ++i) {
          if(grown[i] != entry->pattern) {
            fprintf(stderr, "malloc-stress: realloc pattern mismatch at iter %u\n", iter);
            goto failure;
          }
        }
        fill_pattern(grown, new_size, new_pattern);
        entry->ptr = grown;
        entry->size = new_size;
        entry->pattern = new_pattern;
        ++reallocations;
        continue;
      }

      free(entry->ptr);
      entry->ptr = NULL;
      entry->size = 0;
    }

    size_t size = (lcg_next(&state) % MAX_ALLOC_SIZE) + 1u;
    uint8_t pattern = (uint8_t)size;
    errno = 0;
    uint8_t* block = malloc(size);
    if(block == NULL) {
      fprintf(stderr, "malloc-stress: malloc failure at iter %u (size=%zu, errno=%d)\n",
              iter,
              size,
              errno);
      goto failure;
    }
    fill_pattern(block, size, pattern);
    size_t usable = malloc_usable_size(block);
    if(usable < size) {
      fprintf(stderr, "malloc-stress: usable size %zu < requested %zu\n", usable, size);
      free(block);
      goto failure;
    }

    entry->ptr = block;
    entry->size = size;
    entry->pattern = pattern;
    ++allocations;

    if(((iter + 1u) % 500u) == 0u) {
      printf("  iteration %u/%u\n", iter + 1u, iterations);
    }
  }

  for(size_t i = 0; i < DEFAULT_SLOTS; ++i) {
    if(slots[i].ptr != NULL) {
      if(verify_pattern(&slots[i]) != 0) {
        goto failure;
      }
      free(slots[i].ptr);
    }
  }

  printf("malloc-stress: completed successfully (%u allocations, %u reallocations)\n",
         allocations,
         reallocations);
  return 0;

failure:
  for(size_t i = 0; i < DEFAULT_SLOTS; ++i) {
    free(slots[i].ptr);
  }
  return 1;
}
