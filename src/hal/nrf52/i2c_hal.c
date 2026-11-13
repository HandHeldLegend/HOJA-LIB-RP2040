#include "hal/i2c_hal.h"

#include <string.h>

#include "hal/sys_hal.h"
#include "nrf52_hal.h"

// The two TWIM units, blocking. Their DMA reads and writes RAM only, so bytes to send are copied
// into a buffer first (callers pass const tables that sit in flash).

#define I2C_HAL_MAX_INSTANCES 2
#define I2C_HAL_TX_MAX_LEN    64u
// A transfer that timed out has this long to stop before the unit is switched off and on
#define I2C_HAL_STOP_WAIT_US  500u
// A background read (nrf52_i2c_read_start) is given up after this
#define I2C_HAL_BACKGROUND_US 20000u

#define I2C_HAL_ERROR_GENERIC (-1)
#define I2C_HAL_ERROR_TIMEOUT (-2)

typedef struct
{
    NRF_TWIM_Type *twim;
    uint32_t baud_khz;
    bool ready;
    bool suspended; // The last write ended without a stop
    bool background; // A background read is running
    int background_result;
    size_t background_len;
    uint64_t background_until_us;
    uint8_t tx[I2C_HAL_TX_MAX_LEN];
} i2c_hal_instance_s;

static i2c_hal_instance_s _i2c_hal[I2C_HAL_MAX_INSTANCES] = {
    {.twim = NRF_TWIM0},
    {.twim = NRF_TWIM1},
};

static const i2c_hal_bus_hooks_s *_i2c_hal_hooks[I2C_HAL_MAX_INSTANCES] = {NULL};

static uint32_t _i2c_hal_frequency(uint32_t baud_khz)
{
    if (baud_khz <= 100)
        return TWIM_FREQUENCY_FREQUENCY_K100;
    if (baud_khz <= 250)
        return TWIM_FREQUENCY_FREQUENCY_K250;
    return TWIM_FREQUENCY_FREQUENCY_K400;
}

static void _i2c_hal_pin_init(uint32_t gpio)
{
    // Open drain, with the chip's pull-up beside the board's
    nrf52_gpio_port(gpio)->PIN_CNF[gpio & 31u] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
}

bool i2c_hal_init(uint8_t instance, uint32_t sda, uint32_t scl, uint32_t baudrate_khz)
{
    if (instance >= I2C_HAL_MAX_INSTANCES)
        return false;

    i2c_hal_instance_s *i2c = &_i2c_hal[instance];
    NRF_TWIM_Type *twim = i2c->twim;

    twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
    _i2c_hal_pin_init(sda);
    _i2c_hal_pin_init(scl);
    twim->PSEL.SDA = nrf52_psel(sda);
    twim->PSEL.SCL = nrf52_psel(scl);
    twim->FREQUENCY = _i2c_hal_frequency(baudrate_khz);
    twim->INTENCLR = 0xFFFFFFFFu;
    twim->SHORTS = 0;
    twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;

    i2c->baud_khz = baudrate_khz;
    i2c->suspended = false;
    i2c->ready = true;
    return true;
}

void i2c_hal_deinit(uint8_t instance)
{
    if (instance >= I2C_HAL_MAX_INSTANCES)
        return;

    _i2c_hal[instance].twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
    _i2c_hal[instance].ready = false;
}

void i2c_hal_set_bus_hooks(uint8_t instance, const i2c_hal_bus_hooks_s *hooks)
{
    if (instance < I2C_HAL_MAX_INSTANCES)
        _i2c_hal_hooks[instance] = hooks;
}

uint32_t i2c_hal_get_baudrate(uint8_t instance)
{
    return (instance < I2C_HAL_MAX_INSTANCES) ? _i2c_hal[instance].baud_khz * 1000u : 0;
}

// A unit that won't stop (a device holding the bus) is switched off and on, which drops the transfer
static void _i2c_hal_abort(i2c_hal_instance_s *i2c)
{
    NRF_TWIM_Type *twim = i2c->twim;

    twim->EVENTS_STOPPED = 0;
    twim->TASKS_RESUME = 1;
    twim->TASKS_STOP = 1;

    const uint64_t until = sys_hal_now_us() + I2C_HAL_STOP_WAIT_US;
    while (!twim->EVENTS_STOPPED && (sys_hal_now_us() < until))
    {
    }

    if (!twim->EVENTS_STOPPED)
    {
        twim->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
        twim->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
    }
    twim->EVENTS_STOPPED = 0;
    twim->EVENTS_ERROR = 0;
    twim->ERRORSRC = twim->ERRORSRC;
    i2c->suspended = false;
}

// Where a background read stands: ended (its result kept), or given up when it has run too long
static void _i2c_hal_background_poll(i2c_hal_instance_s *i2c)
{
    NRF_TWIM_Type *twim = i2c->twim;

    if (!i2c->background)
        return;

    if (twim->EVENTS_ERROR)
    {
        i2c->background_result = I2C_HAL_ERROR_GENERIC;
        _i2c_hal_abort(i2c);
    }
    else if (twim->EVENTS_STOPPED)
    {
        i2c->background_result =
            (twim->RXD.AMOUNT == i2c->background_len) ? (int)i2c->background_len : I2C_HAL_ERROR_GENERIC;
    }
    else if (sys_hal_now_us() >= i2c->background_until_us)
    {
        i2c->background_result = I2C_HAL_ERROR_TIMEOUT;
        _i2c_hal_abort(i2c);
    }
    else
        return;

    twim->SHORTS = 0;
    i2c->background = false;
}

bool nrf52_i2c_read_start(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len)
{
    if ((instance >= I2C_HAL_MAX_INSTANCES) || !_i2c_hal[instance].ready || !len || (len > 0xFFFFu))
        return false;

    i2c_hal_instance_s *i2c = &_i2c_hal[instance];
    NRF_TWIM_Type *twim = i2c->twim;

    // Not behind bus hooks, whose holder expects the bus back when a call returns, and not while
    // a write waits to be carried on
    _i2c_hal_background_poll(i2c);
    if (i2c->background || i2c->suspended || _i2c_hal_hooks[instance])
        return false;

    twim->RXD.PTR = (uint32_t)dst;
    twim->RXD.MAXCNT = len;
    twim->FREQUENCY = _i2c_hal_frequency(i2c->baud_khz);
    twim->ADDRESS = addr;
    twim->EVENTS_STOPPED = 0;
    twim->EVENTS_SUSPENDED = 0;
    twim->EVENTS_ERROR = 0;
    twim->SHORTS = TWIM_SHORTS_LASTRX_STOP_Msk;

    i2c->background = true;
    i2c->background_result = 0;
    i2c->background_len = len;
    i2c->background_until_us = sys_hal_now_us() + I2C_HAL_BACKGROUND_US;
    twim->TASKS_STARTRX = 1;
    return true;
}

bool nrf52_i2c_read_busy(uint8_t instance)
{
    if (instance >= I2C_HAL_MAX_INSTANCES)
        return false;
    _i2c_hal_background_poll(&_i2c_hal[instance]);
    return _i2c_hal[instance].background;
}

int nrf52_i2c_read_result(uint8_t instance)
{
    if ((instance >= I2C_HAL_MAX_INSTANCES) || nrf52_i2c_read_busy(instance))
        return 0;
    return _i2c_hal[instance].background_result;
}

// A write, a read, or a write followed by a read under a repeated start. Returns the length of the
// last part, or a negative error.
static int _i2c_hal_transfer(uint8_t instance, uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx,
                             size_t rx_len, bool nostop, int timeout_us, uint32_t baud_khz)
{
    if ((instance >= I2C_HAL_MAX_INSTANCES) || !_i2c_hal[instance].ready)
        return I2C_HAL_ERROR_GENERIC;
    if ((tx_len == 0) && (rx_len == 0))
        return 0;
    if ((tx_len > 0xFFFFu) || (rx_len > 0xFFFFu))
        return I2C_HAL_ERROR_GENERIC;

    i2c_hal_instance_s *i2c = &_i2c_hal[instance];
    NRF_TWIM_Type *twim = i2c->twim;

    // A background read has the unit until it ends (or is given up)
    while (i2c->background)
        _i2c_hal_background_poll(i2c);

    if (_i2c_hal_hooks[instance])
        _i2c_hal_hooks[instance]->acquire();

    if (tx_len)
    {
        // Sent from the buffer (the bytes may be in flash). A longer write must already be in RAM.
        if (tx_len <= I2C_HAL_TX_MAX_LEN)
        {
            memcpy(i2c->tx, tx, tx_len);
            twim->TXD.PTR = (uint32_t)i2c->tx;
        }
        else
            twim->TXD.PTR = (uint32_t)tx;
        twim->TXD.MAXCNT = tx_len;
    }
    if (rx_len)
    {
        twim->RXD.PTR = (uint32_t)rx;
        twim->RXD.MAXCNT = rx_len;
    }

    twim->FREQUENCY = _i2c_hal_frequency(baud_khz ? baud_khz : i2c->baud_khz);
    twim->ADDRESS = addr;
    twim->EVENTS_STOPPED = 0;
    twim->EVENTS_SUSPENDED = 0;
    twim->EVENTS_ERROR = 0;

    // A write left open ends here: this transfer's start is the repeated start
    const bool resume = i2c->suspended;
    const bool suspend = nostop && tx_len && !rx_len;
    i2c->suspended = false;

    if (tx_len && rx_len)
        twim->SHORTS = TWIM_SHORTS_LASTTX_STARTRX_Msk | TWIM_SHORTS_LASTRX_STOP_Msk;
    else if (tx_len)
        twim->SHORTS = suspend ? TWIM_SHORTS_LASTTX_SUSPEND_Msk : TWIM_SHORTS_LASTTX_STOP_Msk;
    else
        twim->SHORTS = TWIM_SHORTS_LASTRX_STOP_Msk;

    if (tx_len)
        twim->TASKS_STARTTX = 1;
    else
        twim->TASKS_STARTRX = 1;
    if (resume)
        twim->TASKS_RESUME = 1;

    int result = (int)(rx_len ? rx_len : tx_len);
    volatile uint32_t *done = suspend ? &twim->EVENTS_SUSPENDED : &twim->EVENTS_STOPPED;
    const uint64_t until = sys_hal_now_us() + (uint64_t)((timeout_us > 0) ? timeout_us : 0);

    for (;;)
    {
        if (twim->EVENTS_ERROR)
        {
            // No answer from the device
            result = I2C_HAL_ERROR_GENERIC;
            _i2c_hal_abort(i2c);
            break;
        }
        if (*done)
        {
            i2c->suspended = suspend;
            break;
        }
        if ((timeout_us >= 0) && (sys_hal_now_us() >= until))
        {
            result = I2C_HAL_ERROR_TIMEOUT;
            _i2c_hal_abort(i2c);
            break;
        }
    }

    twim->SHORTS = 0;

    if (_i2c_hal_hooks[instance])
        _i2c_hal_hooks[instance]->release();

    return result;
}

int i2c_hal_write_timeout_us_odbaud(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                                    int timeout_us, uint32_t baud_khz_override)
{
    return _i2c_hal_transfer(instance, addr, src, len, NULL, 0, nostop, timeout_us, baud_khz_override);
}

int i2c_hal_write_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                             int timeout_us)
{
    return _i2c_hal_transfer(instance, addr, src, len, NULL, 0, nostop, timeout_us, 0);
}

int i2c_hal_read_timeout_us_odbaud(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                                   int timeout_us, uint32_t baud_khz_override)
{
    (void)nostop;
    return _i2c_hal_transfer(instance, addr, NULL, 0, dst, len, false, timeout_us, baud_khz_override);
}

int i2c_hal_read_timeout_us(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us)
{
    (void)nostop;
    return _i2c_hal_transfer(instance, addr, NULL, 0, dst, len, false, timeout_us, 0);
}

// "Blocking" still gives up: a device that holds the bus must not hang the task loop for good
#define I2C_HAL_BLOCKING_TIMEOUT_US 100000

int i2c_hal_write_blocking(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop)
{
    return _i2c_hal_transfer(instance, addr, src, len, NULL, 0, nostop, I2C_HAL_BLOCKING_TIMEOUT_US, 0);
}

int i2c_hal_read_reg_burst_us(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t *dst, size_t len, int timeout_us)
{
    return _i2c_hal_transfer(instance, addr, &reg, 1, dst, len, false, timeout_us, 0);
}

// No background bursts yet: the caller reads them itself
bool i2c_hal_read_reg_burst_async(uint8_t instance, const i2c_hal_burst_s *bursts, uint8_t count)
{
    (void)instance;
    (void)bursts;
    (void)count;
    return false;
}

uint8_t i2c_hal_async_wait(uint8_t instance, int timeout_us)
{
    (void)instance;
    (void)timeout_us;
    return 0;
}

int i2c_hal_write_read_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst,
                                  size_t dst_len, int timeout_us)
{
    return _i2c_hal_transfer(instance, addr, src, wr_len, dst, dst_len, false, timeout_us, 0);
}

int i2c_hal_write_read_blocking(uint8_t instance, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst,
                                size_t dst_len)
{
    return _i2c_hal_transfer(instance, addr, src, wr_len, dst, dst_len, false, I2C_HAL_BLOCKING_TIMEOUT_US, 0);
}
