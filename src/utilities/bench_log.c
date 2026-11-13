#include "utilities/bench_log.h"

#if defined(HOJA_BENCH_LOG)

#include <stdbool.h>

#include "hal/sys_hal.h"

#define BENCH_LOG_LEN       8192u
// A kept log: half the length, in RAM the board's linker script sets aside
#define BENCH_LOG_KEEP_LEN  4096u
#define BENCH_LOG_MAGIC     0x474F4C42u // "BLOG"

typedef struct
{
    uint32_t magic;
    uint16_t head;
    uint16_t tail;
    char text[];
} bench_log_keep_s;

// Where the board gives RAM that a restart leaves alone (__bench_log_keep, room for this header and
// BENCH_LOG_KEEP_LEN of text), the log lives there and outlasts the restart: what a mode with no USB
// logged is read once the gamepad is back in one that has it.
extern bench_log_keep_s __bench_log_keep __attribute__((weak));

static char _bench_log_own[BENCH_LOG_LEN];
static uint16_t _bench_log_own_head = 0;
static uint16_t _bench_log_own_tail = 0;

static char *_bench_log_text = _bench_log_own;
static uint16_t *_bench_log_head = &_bench_log_own_head;
static uint16_t *_bench_log_tail = &_bench_log_own_tail;
static uint16_t _bench_log_len = BENCH_LOG_LEN;
static bool _bench_log_found = false;

// Interrupts off. The kept log is taken as it was left, unless it was never written (or not whole).
static void _bench_log_find(void)
{
    if (_bench_log_found)
        return;
    _bench_log_found = true;

    bench_log_keep_s *keep = &__bench_log_keep;
    if (!keep)
        return;

    if ((keep->magic != BENCH_LOG_MAGIC) || ((uint16_t)(keep->head - keep->tail) > BENCH_LOG_KEEP_LEN))
    {
        keep->magic = BENCH_LOG_MAGIC;
        keep->head = 0;
        keep->tail = 0;
    }
    _bench_log_text = keep->text;
    _bench_log_head = &keep->head;
    _bench_log_tail = &keep->tail;
    _bench_log_len = BENCH_LOG_KEEP_LEN;
}

void bench_log_write(const char *text, size_t len)
{
    const uint32_t state = sys_hal_irq_save();
    _bench_log_find();
    for (size_t i = 0; i < len; i++)
    {
        if ((uint16_t)(*_bench_log_head - *_bench_log_tail) >= _bench_log_len)
            (*_bench_log_tail)++;
        _bench_log_text[((*_bench_log_head)++) % _bench_log_len] = text[i];
    }
    sys_hal_irq_restore(state);
}

uint16_t bench_log_read(uint8_t *out, uint16_t max)
{
    uint16_t n = 0;
    const uint32_t state = sys_hal_irq_save();
    _bench_log_find();
    while ((n < max) && (*_bench_log_tail != *_bench_log_head))
        out[n++] = (uint8_t)_bench_log_text[((*_bench_log_tail)++) % _bench_log_len];
    sys_hal_irq_restore(state);
    return n;
}

uint16_t bench_log_waiting(void)
{
    const uint32_t state = sys_hal_irq_save();
    _bench_log_find();
    const uint16_t n = (uint16_t)(*_bench_log_head - *_bench_log_tail);
    sys_hal_irq_restore(state);
    return n;
}

#else

void bench_log_write(const char *text, size_t len)
{
    (void)text;
    (void)len;
}

uint16_t bench_log_read(uint8_t *out, uint16_t max)
{
    (void)out;
    (void)max;
    return 0;
}

uint16_t bench_log_waiting(void)
{
    return 0;
}

#endif
