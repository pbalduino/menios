#ifndef MENIOS_INCLUDE_KERNEL_ATOMIC_H
#define MENIOS_INCLUDE_KERNEL_ATOMIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t value;
} atomic32_t;

typedef struct {
    volatile uint64_t value;
} atomic64_t;

typedef enum {
    memory_order_relaxed,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order_t;

static inline int atomic_order_to_builtin(memory_order_t order) {
    switch(order) {
        case memory_order_relaxed: return __ATOMIC_RELAXED;
        case memory_order_acquire: return __ATOMIC_ACQUIRE;
        case memory_order_release: return __ATOMIC_RELEASE;
        case memory_order_acq_rel: return __ATOMIC_ACQ_REL;
        case memory_order_seq_cst:
        default:
            return __ATOMIC_SEQ_CST;
    }
}

static inline uint32_t atomic_load32(const atomic32_t* obj, memory_order_t order) {
    return __atomic_load_n(&obj->value, atomic_order_to_builtin(order));
}

static inline void atomic_store32(atomic32_t* obj, uint32_t val, memory_order_t order) {
    __atomic_store_n(&obj->value, val, atomic_order_to_builtin(order));
}

static inline uint32_t atomic_exchange32(atomic32_t* obj, uint32_t val, memory_order_t order) {
    return __atomic_exchange_n(&obj->value, val, atomic_order_to_builtin(order));
}

static inline bool atomic_compare_exchange32(atomic32_t* obj,
                                             uint32_t* expected,
                                             uint32_t desired,
                                             memory_order_t success,
                                             memory_order_t failure) {
    return __atomic_compare_exchange_n(&obj->value,
                                       expected,
                                       desired,
                                       false,
                                       atomic_order_to_builtin(success),
                                       atomic_order_to_builtin(failure));
}

static inline uint32_t atomic_fetch_add32(atomic32_t* obj, uint32_t arg, memory_order_t order) {
    return __atomic_fetch_add(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint32_t atomic_fetch_sub32(atomic32_t* obj, uint32_t arg, memory_order_t order) {
    return __atomic_fetch_sub(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint32_t atomic_fetch_or32(atomic32_t* obj, uint32_t arg, memory_order_t order) {
    return __atomic_fetch_or(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint32_t atomic_fetch_and32(atomic32_t* obj, uint32_t arg, memory_order_t order) {
    return __atomic_fetch_and(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint64_t atomic_load64(const atomic64_t* obj, memory_order_t order) {
    return __atomic_load_n(&obj->value, atomic_order_to_builtin(order));
}

static inline void atomic_store64(atomic64_t* obj, uint64_t val, memory_order_t order) {
    __atomic_store_n(&obj->value, val, atomic_order_to_builtin(order));
}

static inline uint64_t atomic_exchange64(atomic64_t* obj, uint64_t val, memory_order_t order) {
    return __atomic_exchange_n(&obj->value, val, atomic_order_to_builtin(order));
}

static inline bool atomic_compare_exchange64(atomic64_t* obj,
                                             uint64_t* expected,
                                             uint64_t desired,
                                             memory_order_t success,
                                             memory_order_t failure) {
    return __atomic_compare_exchange_n(&obj->value,
                                       expected,
                                       desired,
                                       false,
                                       atomic_order_to_builtin(success),
                                       atomic_order_to_builtin(failure));
}

static inline uint64_t atomic_fetch_add64(atomic64_t* obj, uint64_t arg, memory_order_t order) {
    return __atomic_fetch_add(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint64_t atomic_fetch_sub64(atomic64_t* obj, uint64_t arg, memory_order_t order) {
    return __atomic_fetch_sub(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint64_t atomic_fetch_or64(atomic64_t* obj, uint64_t arg, memory_order_t order) {
    return __atomic_fetch_or(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline uint64_t atomic_fetch_and64(atomic64_t* obj, uint64_t arg, memory_order_t order) {
    return __atomic_fetch_and(&obj->value, arg, atomic_order_to_builtin(order));
}

static inline void memory_barrier(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static inline void read_barrier(void) {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
}

static inline void write_barrier(void) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

static inline void smp_mb(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static inline void smp_rmb(void) {
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
}

static inline void smp_wmb(void) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif
