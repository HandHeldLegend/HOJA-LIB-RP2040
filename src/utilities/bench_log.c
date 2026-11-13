#include "utilities/bench_log.h"

#if defined(HOJA_BENCH_LOG)

#include "hal/sys_hal.h"

#define BENCH_LOG_LEN 8192u

static char _bench_log[BENCH_LOG_LEN];
static uint16_t _bench_log_head = 0;
static uint16_t _bench_log_tail = 0;

void bench_log_write(const char *text, size_t len)
{
    const uint32_t state = sys_hal_irq_save();
    for (size_t i = 0; i < len; i++)
    {
        if ((uint16_t)(_bench_log_head - _bench_log_tail) >= BENCH_LOG_LEN)
            _bench_log_tail++;
        _bench_log[(_bench_log_head++) % BENCH_LOG_LEN] = text[i];
    }
    sys_hal_irq_restore(state);
}

uint16_t bench_log_read(uint8_t *out, uint16_t max)
{
    uint16_t n = 0;
    const uint32_t state = sys_hal_irq_save();
    while ((n < max) && (_bench_log_tail != _bench_log_head))
        out[n++] = (uint8_t)_bench_log[(_bench_log_tail++) % BENCH_LOG_LEN];
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

#endif
