#include "hal/spi_hal.h"

#include <string.h>

#include "hal/gpio_hal.h"
#include "nrf52_hal.h"

// SPIM2 and SPIM3, blocking, mode 0 at 4 MHz. Their DMA reads RAM only, so bytes to send are copied
// into a buffer first.

#define SPI_HAL_MAX_INSTANCES 2
#define SPI_HAL_TX_MAX_LEN    64u
#define SPI_HAL_WAIT_SPINS    1000000u

static NRF_SPIM_Type *const _spi_hal_units[SPI_HAL_MAX_INSTANCES] = {NRF_SPIM2, NRF_SPIM3};
static bool _spi_hal_ready[SPI_HAL_MAX_INSTANCES] = {false};
static uint8_t _spi_hal_tx[SPI_HAL_TX_MAX_LEN];

bool spi_hal_init(uint8_t instance, uint32_t clock, uint32_t miso, uint32_t mosi)
{
    if (instance >= SPI_HAL_MAX_INSTANCES)
        return false;

    NRF_SPIM_Type *spim = _spi_hal_units[instance];

    spim->ENABLE = SPIM_ENABLE_ENABLE_Disabled;

    gpio_hal_init(clock, false, false);
    gpio_hal_write(clock, false);
    gpio_hal_init(mosi, false, false);
    gpio_hal_write(mosi, false);
    gpio_hal_init(miso, false, true);

    spim->PSEL.SCK = nrf52_psel(clock);
    spim->PSEL.MOSI = nrf52_psel(mosi);
    spim->PSEL.MISO = nrf52_psel(miso);
    spim->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M4;
    spim->CONFIG = (SPIM_CONFIG_ORDER_MsbFirst << SPIM_CONFIG_ORDER_Pos) |
                   (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos) |
                   (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos);
    spim->INTENCLR = 0xFFFFFFFFu;
    spim->ENABLE = SPIM_ENABLE_ENABLE_Enabled;

    _spi_hal_ready[instance] = true;
    return true;
}

bool spi_hal_deinit(uint8_t instance)
{
    if (instance >= SPI_HAL_MAX_INSTANCES)
        return false;

    _spi_hal_units[instance]->ENABLE = SPIM_ENABLE_ENABLE_Disabled;
    _spi_hal_ready[instance] = false;
    return true;
}

// One clocked run: tx_len bytes out (their answers dropped), or rx_len bytes in while `fill` goes out
static bool _spi_hal_run(NRF_SPIM_Type *spim, const uint8_t *tx, size_t tx_len, uint8_t fill, uint8_t *rx,
                         size_t rx_len)
{
    while (tx_len || rx_len)
    {
        size_t tx_now = (tx_len > SPI_HAL_TX_MAX_LEN) ? SPI_HAL_TX_MAX_LEN : tx_len;
        size_t rx_now = tx_now ? 0 : ((rx_len > 0xFFFFu) ? 0xFFFFu : rx_len);

        if (tx_now)
            memcpy(_spi_hal_tx, tx, tx_now);
        spim->TXD.PTR = (uint32_t)_spi_hal_tx;
        spim->TXD.MAXCNT = tx_now;
        spim->RXD.PTR = (uint32_t)rx;
        spim->RXD.MAXCNT = rx_now;
        spim->ORC = fill;

        spim->EVENTS_END = 0;
        spim->TASKS_START = 1;
        uint32_t spins = 0;
        while (!spim->EVENTS_END)
        {
            if (++spins > SPI_HAL_WAIT_SPINS)
            {
                spim->TASKS_STOP = 1;
                return false;
            }
        }

        tx += tx_now;
        tx_len -= tx_now;
        rx += rx_now;
        rx_len -= rx_now;
    }
    return true;
}

int spi_hal_read_blocking(uint8_t instance, uint32_t cs_gpio, uint8_t repeated_tx_data, uint8_t *dst, size_t len)
{
    return spi_hal_read_write_blocking(instance, cs_gpio, NULL, 0, repeated_tx_data, dst, len);
}

int spi_hal_write_blocking(uint8_t instance, uint32_t cs_gpio, const uint8_t *data, size_t len)
{
    if ((instance >= SPI_HAL_MAX_INSTANCES) || !_spi_hal_ready[instance])
        return -1;

    gpio_hal_write(cs_gpio, false);
    const bool ok = _spi_hal_run(_spi_hal_units[instance], data, len, 0, NULL, 0);
    gpio_hal_write(cs_gpio, true);

    return ok ? (int)len : -1;
}

int spi_hal_read_write_blocking(uint8_t instance, uint32_t cs_gpio, const uint8_t *data, size_t data_len,
                                uint8_t repeated_tx_data, uint8_t *dst, size_t dst_len)
{
    if ((instance >= SPI_HAL_MAX_INSTANCES) || !_spi_hal_ready[instance])
        return -1;

    NRF_SPIM_Type *spim = _spi_hal_units[instance];

    gpio_hal_write(cs_gpio, false);
    const bool ok = _spi_hal_run(spim, data, data_len, 0, NULL, 0) &&
                    _spi_hal_run(spim, NULL, 0, repeated_tx_data, dst, dst_len);
    gpio_hal_write(cs_gpio, true);

    return ok ? (int)dst_len : -1;
}
