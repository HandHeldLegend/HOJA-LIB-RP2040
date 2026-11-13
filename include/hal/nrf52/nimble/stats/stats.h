#ifndef HOJA_NIMBLE_STATS_H
#define HOJA_NIMBLE_STATS_H

// NimBLE counts what its link layer does (packets sent, anchors missed, CRC errors) through these
// macros. The header its porting folder ships makes them nothing. On the bench (HOJA_BENCH_LOG) they
// are plain 32-bit counters, one after another in each section, read by src/hal/nrf52/ble.

#if defined(HOJA_BENCH_LOG)

#include <stdint.h>
#include <stddef.h>

#define STATS_SECT_DECL(__name)         struct stats_ ## __name
#define STATS_SECT_END                  };
#define STATS_SECT_START(__name)        STATS_SECT_DECL(__name) {
#define STATS_SECT_VAR(__var)           s ## __var
#define STATS_HDR(__sectname)           NULL
#define STATS_SECT_ENTRY(__var)         uint32_t s ## __var;
#define STATS_SECT_ENTRY16(__var)       uint32_t s ## __var;
#define STATS_SECT_ENTRY32(__var)       uint32_t s ## __var;
#define STATS_SECT_ENTRY64(__var)       uint32_t s ## __var;
#define STATS_RESET(__var)
#define STATS_SIZE_INIT_PARMS(__sectvarname, __size) 0, 0
#define STATS_INC(__sectvarname, __var)         ((__sectvarname).s ## __var++)
#define STATS_INCN(__sectvarname, __var, __n)   ((__sectvarname).s ## __var += (__n))
#define STATS_CLEAR(__sectvarname, __var)       ((__sectvarname).s ## __var = 0)
#define STATS_NAME_START(__name)
#define STATS_NAME(__name, __entry)
#define STATS_NAME_END(__name)
#define STATS_NAME_INIT_PARMS(__name)   NULL, 0

static inline int stats_init(void *a, uint8_t b, uint8_t c, void *d, uint8_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return 0;
}

static inline int stats_register(void *a, void *b)
{
    (void)a; (void)b;
    return 0;
}

static inline int stats_init_and_reg(void *a, uint8_t b, uint8_t c, void *d, uint8_t e, void *f)
{
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return 0;
}

#else
#include_next "stats/stats.h"
#endif

#endif
