#include "drivers/adc/mcp3002.h"
#include "board_config.h"
#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"

#define BUFFER_TO_UINT16(buffer) (uint16_t)(((buffer[0] & 0x07) << 9) | buffer[1] << 1 | buffer[2] >> 7)
#define CH0_CONFIG 0xD0
#define CH1_CONFIG 0xF0

bool _mcp_3002_initialized[ADC_CH_MAX] = {0};

// Register MCP3002 driver
bool mcp3002_init_channel(adc_channel_cfg_s *cfg)
{
    if(cfg->ch_local > 1) return false;
    // Get associated driver
    adc_driver_cfg_s *driver   = cfg->driver_cfg;

    uint8_t driver_instance = driver->driver_instance;

    // Max 7 instances
    if(driver_instance>7) return false;

    // If we already initialized this channel CS pin, return
    if(_mcp_3002_initialized[driver_instance]) return true;

    // Init this GPIO
    gpio_hal_init(driver->mcp3002_cfg.cs_gpio, true, false);
    _mcp_3002_initialized[driver_instance] = true;

    return true;
}

uint16_t mcp3002_read_channel(adc_channel_cfg_s *cfg)
{
    uint8_t driver_instance = cfg->driver_cfg->driver_instance;
    if(!_mcp_3002_initialized[driver_instance]) return 0;

    uint8_t buffer[3] = {0};

    uint8_t spi_instance    = cfg->driver_cfg->mcp3002_cfg.spi_instance;
    uint8_t cs_gpio         = cfg->driver_cfg->mcp3002_cfg.cs_gpio;
    uint8_t ch_local        = cfg->ch_local;
    uint8_t ch_invert       = cfg->ch_invert;

    if(cfg->ch_local)
    {
        spi_hal_read_blocking(spi_instance, cs_gpio, CH1_CONFIG, buffer, 3);
    }
    else
    {
        spi_hal_read_blocking(spi_instance, cs_gpio, CH0_CONFIG, buffer, 3);
    }

    return (ch_invert) ? (0xFFF -  BUFFER_TO_UINT16(buffer)) : BUFFER_TO_UINT16(buffer);
}
