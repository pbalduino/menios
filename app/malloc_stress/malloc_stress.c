#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ITERATIONS      2000u
#define DEFAULT_SLOTS            128u
#define DEFAULT_MAX_ALLOC_SIZE 16384u
#define DEFAULT_PROGRESS_STEP     500u
#define MAX_SLOTS              65536u
#define MAX_ITERATIONS      10000000u

typedef struct slot_entry {
  uint8_t* ptr;
  size_t   size;
  uint8_t  pattern;
} slot_entry_t;

typedef struct stress_config {
  unsigned iterations;
  unsigned slots;
  size_t   max_alloc_size;
  unsigned progress_step;
  bool     show_progress;
  unsigned long long target_bytes;
} stress_config_t;

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
      size_t usable = malloc_usable_size(entry->ptr);
      fprintf(stderr,
              "malloc-stress: pattern mismatch at byte %zu (expected %u, got %u) size=%zu usable=%zu ptr=%p\n",
              i,
              (unsigned)entry->pattern,
              (unsigned)entry->ptr[i],
              entry->size,
              usable,
              (void*)entry->ptr);
      return -1;
    }
  }
  return 0;
}

static void print_usage(const char* prog) {
  fprintf(stderr,
          "Usage: %s [iterations]\n"
          "       %s [--iterations=N] [--slots=N] [--max-size=N]\n"
          "          [--target-bytes=N] [--target-mb=N] [--progress[=N]]\n"
          "\n"
          "Options:\n"
          "  --iterations=N    Override the number of iterations (default %u).\n"
          "  --slots=N         Number of allocation slots to shuffle (default %u).\n"
          "  --max-size=N      Maximum allocation size in bytes (default %u).\n"
          "  --target-bytes=N  Aim to keep at least N bytes allocated.\n"
          "  --target-mb=N     Convenience wrapper for --target-bytes=N*1024*1024.\n"
          "  --progress[=N]    Print progress every N iterations (default %u).\n"
          "                   Use --progress=0 to disable progress output.\n"
          "  --help            Show this message.\n",
          prog,
          prog,
          DEFAULT_ITERATIONS,
          DEFAULT_SLOTS,
          DEFAULT_MAX_ALLOC_SIZE,
          DEFAULT_PROGRESS_STEP);
}

static const char* match_option(const char* arg, const char* option) {
  size_t len = strlen(option);
  if(strncmp(arg, option, len) != 0) {
    return NULL;
  }
  if(arg[len] == '\0') {
    return "";
  }
  if(arg[len] == '=') {
    return arg + len + 1u;
  }
  return NULL;
}

static int parse_unsigned_value(const char* option,
                                const char* value,
                                unsigned long long min,
                                unsigned long long max,
                                unsigned long long* out) {
  if(value == NULL || *value == '\0') {
    fprintf(stderr, "malloc-stress: option '%s' expects a value\n", option);
    return -1;
  }

  unsigned long long parsed = 0ull;
  for(const char* p = value; *p != '\0'; ++p) {
    if(*p < '0' || *p > '9') {
      fprintf(stderr,
              "malloc-stress: invalid numeric value '%s' for option '%s'\n",
              value,
              option);
      return -1;
    }

    unsigned digit = (unsigned)(*p - '0');
    if(parsed > (max - digit) / 10ull) {
      fprintf(stderr,
              "malloc-stress: value '%s' for option '%s' exceeds supported range\n",
              value,
              option);
      return -1;
    }
    parsed = parsed * 10ull + digit;
  }

  if(parsed < min || parsed > max) {
    fprintf(stderr,
            "malloc-stress: value %llu for option '%s' outside allowed range [%llu, %llu]\n",
            parsed,
            option,
            min,
            max);
    return -1;
  }

  *out = parsed;
  return 0;
}

static int parse_arguments(int argc, char** argv, stress_config_t* config) {
  bool iterations_set = false;

  for(int i = 1; i < argc; ++i) {
    const char* arg = argv[i];

    if(strcmp(arg, "--help") == 0) {
      print_usage(argv[0]);
      return 1;
    }

    if(arg[0] != '-') {
      if(iterations_set) {
        fprintf(stderr, "malloc-stress: unexpected positional argument '%s'\n", arg);
        return -1;
      }

      unsigned long long parsed = 0;
      if(parse_unsigned_value("iterations", arg, 1ull, MAX_ITERATIONS, &parsed) != 0) {
        return -1;
      }
      config->iterations = (unsigned)parsed;
      iterations_set = true;
      continue;
    }

    const char* value = NULL;

    if((value = match_option(arg, "--iterations")) != NULL) {
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--iterations", value, 1ull, MAX_ITERATIONS, &parsed) != 0) {
        return -1;
      }
      config->iterations = (unsigned)parsed;
      iterations_set = true;
      continue;
    }

    if((value = match_option(arg, "--slots")) != NULL) {
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--slots", value, 1ull, MAX_SLOTS, &parsed) != 0) {
        return -1;
      }
      config->slots = (unsigned)parsed;
      continue;
    }

    if((value = match_option(arg, "--max-size")) != NULL) {
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--max-size", value, 1ull, (unsigned long long)SIZE_MAX, &parsed) != 0) {
        return -1;
      }
      config->max_alloc_size = (size_t)parsed;
      continue;
    }

    if((value = match_option(arg, "--target-bytes")) != NULL) {
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--target-bytes", value, 1ull, (unsigned long long)SIZE_MAX, &parsed) != 0) {
        return -1;
      }
      config->target_bytes = parsed;
      continue;
    }

    if((value = match_option(arg, "--target-mb")) != NULL) {
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--target-mb", value, 1ull, (unsigned long long)(SIZE_MAX / (1024ull * 1024ull)), &parsed) != 0) {
        return -1;
      }
      config->target_bytes = parsed * 1024ull * 1024ull;
      continue;
    }

    if((value = match_option(arg, "--progress")) != NULL) {
      if(value[0] == '\0') {
        config->show_progress = true;
        continue;
      }
      unsigned long long parsed = 0;
      if(parse_unsigned_value("--progress", value, 0ull, (unsigned long long)UINT_MAX, &parsed) != 0) {
        return -1;
      }
      config->show_progress = parsed != 0;
      config->progress_step = (unsigned)parsed;
      continue;
    }

    fprintf(stderr, "malloc-stress: unknown option '%s'\n", arg);
    return -1;
  }

  return 0;
}

int main(int argc, char** argv) {
  stress_config_t config = {
    .iterations = DEFAULT_ITERATIONS,
    .slots = DEFAULT_SLOTS,
    .max_alloc_size = DEFAULT_MAX_ALLOC_SIZE,
    .progress_step = DEFAULT_PROGRESS_STEP,
    .show_progress = true,
    .target_bytes = 0ull,
  };

  int parse_rc = parse_arguments(argc, argv, &config);
  if(parse_rc > 0) {
    return 0;
  }
  if(parse_rc < 0) {
    return 1;
  }

  if(config.max_alloc_size == 0u) {
    config.max_alloc_size = 1u;
  }

  slot_entry_t* slots = calloc(config.slots, sizeof(slot_entry_t));
  if(slots == NULL) {
    fprintf(stderr, "malloc-stress: failed to allocate %u slots\n", config.slots);
    return 1;
  }

  unsigned long long capacity = (unsigned long long)config.max_alloc_size * (unsigned long long)config.slots;
  if(config.target_bytes > 0ull && config.target_bytes > capacity) {
    fprintf(stderr,
            "malloc-stress: target %llu bytes exceeds slot capacity %llu bytes; clamping target.\n",
            config.target_bytes,
            capacity);
    config.target_bytes = capacity;
  }

  uint32_t state = 0xC0FFEEu;
  unsigned reallocations = 0;
  unsigned allocations = 0;
  size_t current_usage = 0u;

  printf("malloc-stress: running up to %u iterations across %u slots (max alloc %lu bytes",
         config.iterations,
         config.slots,
         (unsigned long)config.max_alloc_size);
  if(config.target_bytes > 0ull) {
    printf(", target %llu bytes", config.target_bytes);
  }
  printf(")\n");

  unsigned long long iter = 0;

  while(iter < config.iterations || (config.target_bytes > 0ull && current_usage < config.target_bytes)) {
    ++iter;

    uint32_t r = lcg_next(&state);
    size_t index = r % config.slots;
    slot_entry_t* entry = &slots[index];

    if(entry->ptr != NULL) {
      if(verify_pattern(entry) != 0) {
        goto failure;
      }

      if((r & 0x7u) == 0u) {
        size_t new_size = (lcg_next(&state) % config.max_alloc_size) + 1u;
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
        if(new_size >= entry->size) {
          current_usage += (new_size - entry->size);
        } else {
          current_usage -= (entry->size - new_size);
        }
        entry->ptr = grown;
        entry->size = new_size;
        entry->pattern = new_pattern;
        ++reallocations;
        continue;
      }

      free(entry->ptr);
      if(current_usage >= entry->size) {
        current_usage -= entry->size;
      } else {
        current_usage = 0;
      }
      entry->ptr = NULL;
      entry->size = 0;
    }

    size_t size = (lcg_next(&state) % config.max_alloc_size) + 1u;
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
    current_usage += size;
    ++allocations;

    if(config.show_progress && config.progress_step > 0u && (iter % config.progress_step) == 0u) {
      const unsigned long long limit = config.iterations;
      const unsigned long usage = (unsigned long)current_usage;
      if(config.target_bytes > 0ull) {
        if(iter <= limit) {
          printf("  iteration %llu/%u (usage=%lu bytes, target=%llu)\n",
                 iter,
                 config.iterations,
                 usage,
                 config.target_bytes);
        } else {
          printf("  iteration %llu (usage=%lu bytes, target=%llu)\n",
                 iter,
                 usage,
                 config.target_bytes);
        }
      } else {
        if(iter <= limit) {
          printf("  iteration %llu/%u\n", iter, config.iterations);
        } else {
          printf("  iteration %llu\n", iter);
        }
      }
    }
  }

  for(size_t i = 0; i < config.slots; ++i) {
    if(slots[i].ptr != NULL) {
      if(verify_pattern(&slots[i]) != 0) {
        goto failure;
      }
      if(current_usage >= slots[i].size) {
        current_usage -= slots[i].size;
      } else {
        current_usage = 0;
      }
      free(slots[i].ptr);
    }
  }

  printf("malloc-stress: completed successfully (%u allocations, %u reallocations)\n",
         allocations,
         reallocations);
  free(slots);
  return 0;

failure:
  for(size_t i = 0; i < config.slots; ++i) {
    free(slots[i].ptr);
  }
  free(slots);
  return 1;
}
