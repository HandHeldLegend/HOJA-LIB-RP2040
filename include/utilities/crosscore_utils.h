/*
 * Portable cross-core primitives for the HOJA device library.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file crosscore_utils.h
 * @brief Self-contained SPSC snapshot + FIFO templates for cross-task / cross-core sharing.
 *
 * Two strict single-producer / single-consumer (SPSC) primitives:
 *
 *   - HOJA_CROSSCORE_SNAPSHOT_TYPE(name, type): seqlock "latest value" cell. Use when only
 *     the most recent value matters and intermediate updates may be coalesced.
 *
 *   - HOJA_CROSSCORE_FIFO_TYPE(name, type, len): lock-free ring buffer. Use when every
 *     element must be preserved in order.
 *
 * Macro contract (identical for both primitives):
 *   - EXACTLY ONE context may call *_write / *_push (the producer).
 *   - EXACTLY ONE context may call *_read  / *_pop  (the consumer).
 *   - `len` for a FIFO MUST be a power of two.
 *
 * SNAPSHOT_TYPE(name, type) is a legacy alias that generates snapshot_* symbols used
 * throughout existing firmware; new code should prefer HOJA_CROSSCORE_* macros.
 */

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Seqlock-style "latest value" snapshot                                      */
/* -------------------------------------------------------------------------- */

#define _HOJA_CROSSCORE_SNAPSHOT_TYPE_IMPL(prefix, name, type)                 \
typedef struct {                                                               \
    atomic_uint seq;                                                           \
    type        data;                                                          \
    type        stale_data;                                                    \
} prefix##_##name##_t;                                                         \
                                                                               \
static inline void prefix##_##name##_write(prefix##_##name##_t *s,             \
                                           const type *src)                    \
{                                                                              \
    unsigned int seq0 = atomic_load_explicit(&s->seq, memory_order_relaxed);   \
    atomic_store_explicit(&s->seq, seq0 + 1u, memory_order_relaxed);         \
    atomic_thread_fence(memory_order_release);                                 \
    memcpy(&s->data, src, sizeof(type));                                       \
    atomic_thread_fence(memory_order_release);                                 \
    atomic_store_explicit(&s->seq, seq0 + 2u, memory_order_release);           \
    memcpy(&s->stale_data, src, sizeof(type));                                 \
}                                                                              \
                                                                               \
static inline bool prefix##_##name##_read(prefix##_##name##_t *s, type *dst)   \
{                                                                              \
    unsigned int s1 = atomic_load_explicit(&s->seq, memory_order_acquire);    \
    if (s1 & 1u) {                                                             \
        memcpy(dst, &s->stale_data, sizeof(type));                             \
        return false;                                                          \
    }                                                                          \
    memcpy(dst, &s->data, sizeof(type));                                       \
    atomic_thread_fence(memory_order_acquire);                                 \
    unsigned int s2 = atomic_load_explicit(&s->seq, memory_order_acquire);    \
    if (s1 != s2 || (s2 & 1u)) {                                              \
        memcpy(dst, &s->stale_data, sizeof(type));                             \
        return false;                                                          \
    }                                                                          \
    return true;                                                               \
}

#define HOJA_CROSSCORE_SNAPSHOT_TYPE(name, type)                               \
    _HOJA_CROSSCORE_SNAPSHOT_TYPE_IMPL(hoja_snapshot, name, type)

#define SNAPSHOT_TYPE(name, type)                                              \
    _HOJA_CROSSCORE_SNAPSHOT_TYPE_IMPL(snapshot, name, type)

/* -------------------------------------------------------------------------- */
/* Lock-free SPSC ring buffer FIFO                                            */
/* -------------------------------------------------------------------------- */

#define HOJA_CROSSCORE_FIFO_TYPE(name, type, len)                              \
typedef struct {                                                               \
    type        buf[len];                                                      \
    atomic_uint head;                                                          \
    atomic_uint tail;                                                          \
} hoja_fifo_##name##_t;                                                        \
                                                                               \
static inline bool hoja_fifo_##name##_push(hoja_fifo_##name##_t *f,            \
                                           const type *src)                    \
{                                                                              \
    unsigned int t = atomic_load_explicit(&f->tail, memory_order_relaxed);     \
    unsigned int h = atomic_load_explicit(&f->head, memory_order_acquire);     \
    if ((unsigned int)(t - h) >= (len)) return false;                          \
    memcpy(&f->buf[t & ((len) - 1u)], src, sizeof(type));                      \
    atomic_store_explicit(&f->tail, t + 1u, memory_order_release);             \
    return true;                                                               \
}                                                                              \
                                                                               \
static inline bool hoja_fifo_##name##_pop(hoja_fifo_##name##_t *f, type *dst)  \
{                                                                              \
    unsigned int h = atomic_load_explicit(&f->head, memory_order_relaxed);     \
    unsigned int t = atomic_load_explicit(&f->tail, memory_order_acquire);     \
    if (h == t) return false;                                                  \
    memcpy(dst, &f->buf[h & ((len) - 1u)], sizeof(type));                      \
    atomic_store_explicit(&f->head, h + 1u, memory_order_release);             \
    return true;                                                               \
}
