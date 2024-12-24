#ifndef MENIOS_INCLUDE_KERNEL_THREAD_H
#define MENIOS_INCLUDE_KERNEL_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_RUNNING    0
#define THREAD_TERMINATED 1

typedef struct kthread_t {
  const char* name;
  int         (*entrypoint)(void*);
  void*       arguments;
  int         status;
} kthread_t;

typedef kthread_t* kthread_p;

// typedef void *(*entrypoint)(void *) kthread_handler_t;

int kthread_create(kthread_t* thread, const char* name, int (*entrypoint)(void *), void* arg);

void ksleep(uint64_t milliseconds);
void kexit(int code);

#ifdef __cplusplus
}
#endif

#endif //MENIOS_INCLUDE_KERNEL_THREAD_H