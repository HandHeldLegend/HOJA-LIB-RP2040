#include "drivers/adc/mcp3002.h"
#include "board_config.h"
#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"

#define BUFFER_TO_UINT16(buffer) (uint16_t)(((buffer[0] & 0x07) << 9) | buffer[1] << 1 | buffer[2] >> 7)
#define CH0_CONFIG 0xD0
#define CH1_CONFIG 0xF0

adc_mcp3002_cfg_t   _mcp_3002_drivers[ADC_CH_MAX] = {0};
bool                _mcp_3002_initialized[ADC_CH_MAX] = {0};

// Register MCP3002 driver
bool mcp3002_register_driver(uint8_t ch_local, adc_driver_cfg_s *cfg)
{
    if(cfg->driver_instance<0) return false;
    int8_t instance = cfg->driver_instance;

    // Get associated driver
    adc_mcp3002_cfg_t *driver   = &_mcp_3002_drivers[instance];
    adc_mcp3002_cfg_t *read     = &cfg->mcp3002_cfg;

    if(_mcp_3002_initialized[instance]) return true;

    driver->cs_gpio         = read->cs_gpio;
    driver->spi_instance    = read->spi_instance;

    // Init this GPIO
    gpio_hal_init(driver->cs_gpio, true, false);
    _mcp_3002_initialized[instance] = true;

    return true;
}

uint16_t mcp3002_read(uint8_t ch_local, uint8_t driver_instance)
{
    if(!_mcp_3002_initialized[driver_instance]) return 0;

    adc_mcp3002_cfg_t *cfg = &_mcp_3002_drivers[driver_instance];

    uint8_t buffer[3] = {0};

    if(ch_local)
    {
        spi_hal_read_blocking(cfg->spi_instance, cfg->cs_gpio, CH1_CONFIG, buffer, 3);
    }
    else
    {
        spi_hal_read_blocking(cfg->spi_instance, cfg->cs_gpio, CH0_CONFIG, buffer, 3);
    }

    return BUFFER_TO_UINT16(buffer);
}
