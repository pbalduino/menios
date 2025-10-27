#include <stddef.h>

#include <stdio.h>
#include <stdlib.h>

void __menios_env_init(int argc, char** argv, char** envp);
void __menios_env_fini(void);
void __menios_atexit_run(void);

void __menios_init_libc(int argc, char** argv, char** envp) {
  __menios_env_init(argc, argv, envp);
}

void __menios_fini_libc(int status) {
  (void)status;
  fflush(NULL);
  __menios_atexit_run();
  __menios_env_fini();
}
