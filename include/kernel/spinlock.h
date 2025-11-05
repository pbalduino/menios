#ifndef MENIOS_INCLUDE_KERNEL_SPINLOCK_H
#define MENIOS_INCLUDE_KERNEL_SPINLOCK_H

#include <kernel/atomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef MENIOS_KERNEL
#include <kernel/kernel.h>
#include <kernel/serial.h>
#else
#include <stdarg.h>
#include <stdio.h>

static inline void serial_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spinlock_t {
    atomic32_t state;
} spinlock_t;

static inline void spinlock_init(spinlock_t* lock) {
    atomic_store32(&lock->state, 0, memory_order_relaxed);
}

static inline bool spinlock_is_locked(const spinlock_t* lock) {
    return atomic_load32(&lock->state, memory_order_relaxed) != 0;
}

static inline void spinlock_cpu_relax(void) {
#if defined(__x86_64__)
    __asm__ volatile("pause");
#else
    __asm__ volatile("nop");
#endif
}

static inline void spinlock_lock(spinlock_t* lock) {
    const uintptr_t kernel_pointer_floor = 0xffff800000000000ull;
    if(lock == NULL || (uintptr_t)lock < kernel_pointer_floor) {
        serial_printf("spinlock_lock: suspicious lock pointer=%p\n", (void*)lock);
    }
    for(;;) {
        if(atomic_exchange32(&lock->state, 1, memory_order_acquire) == 0) {
            return;
        }
        while(spinlock_is_locked(lock)) {
            spinlock_cpu_relax();
        }
    }
}

static inline bool spinlock_trylock(spinlock_t* lock) {
    return atomic_exchange32(&lock->state, 1, memory_order_acquire) == 0;
}

static inline void spinlock_unlock(spinlock_t* lock) {
    atomic_store32(&lock->state, 0, memory_order_release);
}

#ifdef MENIOS_KERNEL
static inline uint64_t spinlock_lock_irqsave(spinlock_t* lock) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
    disable_interrupts();
    spinlock_lock(lock);
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    spinlock_unlock(lock);
    if(flags & (1ull << 9)) {
        enable_interrupts();
    }
}
#endif

#ifdef __cplusplus
}
#endif

#endif
