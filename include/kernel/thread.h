#ifndef MENIOS_INCLUDE_KERNEL_THREAD_H
#define MENIOS_INCLUDE_KERNEL_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kthread_t {
  const char* name;
  void        (*entrypoint)(void*);
  void*       arguments;
} kthread_t;

typedef kthread_t* kthread_p;

// typedef void *(*entrypoint)(void *) kthread_handler_t;

int kthread_create(kthread_t* thread, const char* name, void (*entrypoint)(void *), void* arg);

void ksleep(uint64_t milliseconds);
void kexit(int code);

#ifdef __cplusplus
}
#endif

#endif //MENIOS_INCLUDE_KERNEL_THREAD_H