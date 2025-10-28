#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#ifndef MENIOS_KERNEL

char** environ = NULL;

static char**           env_entries      = NULL;
static unsigned char*   env_owned        = NULL;
static size_t           env_count        = 0;
static size_t           env_capacity     = 0;
static bool             env_storage_owned = false;

static int env_clone_into_owned(size_t extra_slots);
static int env_ensure_capacity(size_t extra_slots);
static int env_find_index(const char* name, size_t name_len);
static bool env_is_valid_name(const char* name);
static char* env_duplicate_entry(const char* name, size_t name_len, const char* value);
static void env_release_entry(size_t index);

void __menios_env_init(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;

  env_entries = NULL;
  env_owned = NULL;
  env_capacity = 0;
  env_storage_owned = false;

  environ = envp;

  size_t count = 0;
  if(envp != NULL) {
    while(envp[count] != NULL) {
      uintptr_t entry_addr = (uintptr_t)envp[count];
      if(entry_addr < 0x1000u) {
        break;
      }
      count++;
    }
  }
  env_count = count;
}

void __menios_env_fini(void) {
  if(env_storage_owned && env_entries != NULL) {
    for(size_t i = 0; i < env_count; i++) {
      if(env_entries[i] != NULL && env_owned != NULL && env_owned[i]) {
        free(env_entries[i]);
      }
    }
    free(env_entries);
    free(env_owned);
  }

  env_entries = NULL;
  env_owned = NULL;
  environ = NULL;
  env_count = 0;
  env_capacity = 0;
  env_storage_owned = false;
}

char* getenv(const char* name) {
  if(name == NULL || !env_is_valid_name(name)) {
    errno = EINVAL;
    return NULL;
  }

  size_t name_len = 0;
  while(name[name_len] != '\0') {
    name_len++;
  }

  char** current_env = environ;
  if(current_env == NULL) {
    return NULL;
  }

  for(size_t i = 0; current_env[i] != NULL; i++) {
    const char* entry = current_env[i];
    if(strncmp(entry, name, name_len) == 0 && entry[name_len] == '=') {
      return (char*)(entry + name_len + 1);
    }
  }

  return NULL;
}

static int env_find_index(const char* name, size_t name_len) {
  if(environ == NULL) {
    return -1;
  }

  for(size_t i = 0; environ[i] != NULL; i++) {
    const char* entry = environ[i];
    if(strncmp(entry, name, name_len) == 0 && entry[name_len] == '=') {
      return (int)i;
    }
  }

  return -1;
}

static bool env_is_valid_name(const char* name) {
  if(name == NULL || *name == '\0') {
    return false;
  }
  for(const char* it = name; *it != '\0'; ++it) {
    if(*it == '=') {
      return false;
    }
  }
  return true;
}

static char* env_duplicate_entry(const char* name, size_t name_len, const char* value) {
  size_t value_len = value ? strlen(value) : 0;
  size_t total = name_len + 1u + value_len + 1u;
  char* entry = (char*)malloc(total);
  if(entry == NULL) {
    return NULL;
  }

  memcpy(entry, name, name_len);
  entry[name_len] = '=';
  if(value_len > 0) {
    memcpy(entry + name_len + 1u, value, value_len);
  }
  entry[total - 1u] = '\0';
  return entry;
}

static int env_clone_into_owned(size_t extra_slots) {
  size_t required = env_count + extra_slots;
  size_t new_capacity = (required < 8u) ? 8u : required + 4u;

  char** new_entries = (char**)malloc(sizeof(char*) * (new_capacity + 1u));
  unsigned char* new_owned = (unsigned char*)malloc(sizeof(unsigned char) * (new_capacity + 1u));
  if(new_entries == NULL || new_owned == NULL) {
    free(new_entries);
    free(new_owned);
    errno = ENOMEM;
    return -1;
  }

  for(size_t i = 0; i < env_count; i++) {
    const char* source = (environ != NULL) ? environ[i] : NULL;
    if(source == NULL) {
      new_entries[i] = NULL;
      new_owned[i] = 0u;
      continue;
    }
    char* copy = strdup(source);
    if(copy == NULL) {
      for(size_t j = 0; j < i; j++) {
        if(new_owned[j]) {
          free(new_entries[j]);
        }
      }
      free(new_entries);
      free(new_owned);
      errno = ENOMEM;
      return -1;
    }
    new_entries[i] = copy;
    new_owned[i] = 1u;
  }

  new_entries[env_count] = NULL;
  new_owned[env_count] = 0u;

  environ = new_entries;
  env_entries = new_entries;
  env_owned = new_owned;
  env_capacity = new_capacity;
  env_storage_owned = true;
  return 0;
}

static int env_ensure_capacity(size_t extra_slots) {
  if(!env_storage_owned) {
    return env_clone_into_owned(extra_slots);
  }

  if(env_count + extra_slots < env_capacity) {
    return 0;
  }

  size_t required = env_count + extra_slots;
  size_t new_capacity = env_capacity ? env_capacity * 2u : (required + 4u);
  if(new_capacity < required + 4u) {
    new_capacity = required + 4u;
  }

  char** new_entries = (char**)reallocarray(env_entries, new_capacity + 1u, sizeof(char*));
  unsigned char* new_owned = (unsigned char*)reallocarray(env_owned, new_capacity + 1u, sizeof(unsigned char));
  if(new_entries == NULL || new_owned == NULL) {
    errno = ENOMEM;
    return -1;
  }

  env_entries = new_entries;
  env_owned = new_owned;
  environ = new_entries;
  env_capacity = new_capacity;
  return 0;
}

static void env_release_entry(size_t index) {
  if(env_storage_owned && env_entries != NULL && env_owned != NULL && env_owned[index]) {
    free(env_entries[index]);
  }
}

int setenv(const char* name, const char* value, int overwrite) {
  if(!env_is_valid_name(name)) {
    errno = EINVAL;
    return -1;
  }

  size_t name_len = strlen(name);
  int existing = env_find_index(name, name_len);

  if(existing >= 0 && !overwrite) {
    return 0;
  }

  if(env_ensure_capacity(1u) < 0) {
    return -1;
  }

  char* entry = env_duplicate_entry(name, name_len, value);
  if(entry == NULL) {
    errno = ENOMEM;
    return -1;
  }

  if(existing >= 0) {
    env_release_entry((size_t)existing);
    env_entries[existing] = entry;
    env_owned[existing] = 1u;
    return 0;
  }

  env_entries[env_count] = entry;
  env_owned[env_count] = 1u;
  env_count++;
  env_entries[env_count] = NULL;
  env_owned[env_count] = 0u;
  return 0;
}

int putenv(char* string) {
  if(string == NULL) {
    errno = EINVAL;
    return -1;
  }

  char* equals = strchr(string, '=');
  if(equals == NULL || equals == string) {
    errno = EINVAL;
    return -1;
  }

  size_t name_len = (size_t)(equals - string);

  if(env_ensure_capacity(1u) < 0) {
    return -1;
  }

  int existing = env_find_index(string, name_len);
  if(existing >= 0) {
    env_release_entry((size_t)existing);
    env_entries[existing] = string;
    env_owned[existing] = 0u;
    return 0;
  }

  env_entries[env_count] = string;
  env_owned[env_count] = 0u;
  env_count++;
  env_entries[env_count] = NULL;
  env_owned[env_count] = 0u;
  return 0;
}

int unsetenv(const char* name) {
  if(!env_is_valid_name(name)) {
    errno = EINVAL;
    return -1;
  }

  if(env_ensure_capacity(0u) < 0) {
    return -1;
  }

  size_t name_len = strlen(name);
  int idx = env_find_index(name, name_len);
  if(idx < 0) {
    return 0;
  }

  size_t index = (size_t)idx;
  env_release_entry(index);

  for(size_t i = index; i + 1 < env_count; i++) {
    env_entries[i] = env_entries[i + 1];
    env_owned[i] = env_owned[i + 1];
  }

  if(env_count > 0) {
    env_count--;
    env_entries[env_count] = NULL;
    env_owned[env_count] = 0u;
  }

  return 0;
}

int clearenv(void) {
  if(env_ensure_capacity(0u) < 0) {
    return -1;
  }

  for(size_t i = 0; i < env_count; i++) {
    env_release_entry(i);
    env_entries[i] = NULL;
    env_owned[i] = 0u;
  }
  env_count = 0;
  env_entries[0] = NULL;
  env_owned[0] = 0u;
  return 0;
}

#endif
