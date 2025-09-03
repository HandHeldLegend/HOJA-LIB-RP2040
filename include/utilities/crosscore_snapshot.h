// crosscore_snapshot.h
#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

// Generic container for snapshot-protected data
#define SNAPSHOT_TYPE(name, type)        \
typedef struct {                         \
    atomic_uint seq;                     \
    type data;                           \
} snapshot_##name##_t;                   \
                                         \
static inline void snapshot_##name##_write(snapshot_##name##_t *s, const type *src) { \
    unsigned int seq0 = atomic_load_explicit(&s->seq, memory_order_relaxed); \
    atomic_store_explicit(&s->seq, seq0 + 1, memory_order_relaxed); /* odd = writing */ \
    atomic_thread_fence(memory_order_release);                       \
    memcpy(&s->data, src, sizeof(type));                             \
    atomic_thread_fence(memory_order_release);                       \
    atomic_store_explicit(&s->seq, seq0 + 2, memory_order_release); /* even = stable */ \
}                                                                    \
                                                                     \
static inline bool snapshot_##name##_read(snapshot_##name##_t *s, type *dst) { \
    unsigned int s1, s2;                                             \
    do {                                                             \
        s1 = atomic_load_explicit(&s->seq, memory_order_acquire);    \
        if (s1 & 1) continue; /* writer in progress */               \
        memcpy(dst, &s->data, sizeof(type));                         \
        atomic_thread_fence(memory_order_acquire);                   \
        s2 = atomic_load_explicit(&s->seq, memory_order_acquire);    \
    } while (s1 != s2 || (s2 & 1));                                  \
    return true;                                                     \
}
