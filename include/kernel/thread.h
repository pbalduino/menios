#ifndef MENIOS_INCLUDE_KERNEL_THREAD_H
#define MENIOS_INCLUDE_KERNEL_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kthread_t {

} kthread_t;

// typedef void *(*entrypoint)(void *) kthread_handler_t;

int kthread_create(kthread_t* thread, void (*entrypoint)(void *), void* arg);

#ifdef __cplusplus
}
#endif

#endif //MENIOS_INCLUDE_KERNEL_THREAD_H