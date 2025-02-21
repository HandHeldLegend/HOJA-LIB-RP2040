#include "drivers/adc/tmux1204.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"

#include "drivers/adc/mcp3002.h"
#include "hal/adc_hal.h"
#include <stdlib.h>

bool                _tmux1204_initialized[ADC_CH_MAX]       = {0};
adc_read_fn_t       _tmux1204_read_functions[ADC_CH_MAX]    = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
adc_channel_cfg_s   _tmux1204_nested_channels[ADC_CH_MAX]   = {0};

bool tmux1204_init_channel(adc_channel_cfg_s *cfg)
{
    if(cfg->ch_local > 3) return false; 

    // Get associated driver
    adc_driver_cfg_s *driver = cfg->driver_cfg;
    // Get instance
    uint8_t driver_instance = driver->driver_instance;

    // Only initialize one of each driver instance
    if(_tmux1204_initialized[driver_instance]) return true;

    // Get nested driver layer
    adc_driver_cfg_s *nested = driver->tmux1204_cfg.host_cfg;

    // Save this channel config for later reading use
    _tmux1204_nested_channels[driver_instance].ch_invert    = cfg->ch_invert;
    _tmux1204_nested_channels[driver_instance].driver_cfg   = driver->tmux1204_cfg.host_cfg;
    _tmux1204_nested_channels[driver_instance].ch_local     = driver->tmux1204_cfg.host_ch_local;

    bool nested_init = false;

    // Initialize this base driver for this TMUX chip
    switch(nested->driver_type)
    {
        default:
        return false;

        case ADC_DRIVER_HAL:
        if(adc_hal_init_channel(&_tmux1204_nested_channels[driver_instance]))
        {
            _tmux1204_read_functions[driver_instance] = adc_hal_read_channel;
            nested_init = true;
        }  
        break;

        case ADC_DRIVER_MCP3002:
        if(mcp3002_init_channel(&_tmux1204_nested_channels[driver_instance]))
        {
            _tmux1204_read_functions[driver_instance] = mcp3002_read_channel;
            nested_init = true;
        }  
        break;
    }

    uint8_t a0_gpio = driver->tmux1204_cfg.a0_gpio;
    uint8_t a1_gpio = driver->tmux1204_cfg.a1_gpio;

    // Init our local GPIO
    gpio_hal_init(a0_gpio, true, false);
    gpio_hal_init(a1_gpio, true, false);
    _tmux1204_initialized[driver_instance] = true;
    return true;
}

uint16_t    tmux1204_read_channel(adc_channel_cfg_s *cfg)
{
    uint8_t driver_instance = cfg->driver_cfg->driver_instance;
    if(!_tmux1204_initialized[driver_instance]) return 0;

    uint8_t ch_local = cfg->ch_local;

    uint8_t a0_gpio = cfg->driver_cfg->tmux1204_cfg.a0_gpio;
    uint8_t a1_gpio = cfg->driver_cfg->tmux1204_cfg.a1_gpio;

    // Set MUX position based on channel config
    switch(ch_local)
    {
        // S1
        case 0:
            gpio_hal_write(a0_gpio, false);
            gpio_hal_write(a1_gpio, false);
        break;

        // S2
        case 1:
            gpio_hal_write(a0_gpio, true);
            gpio_hal_write(a1_gpio, false);
        break;

        // S3
        case 2:
            gpio_hal_write(a0_gpio, false);
            gpio_hal_write(a1_gpio, true);
        break;

        // S4
        case 3:
            gpio_hal_write(a0_gpio, true);
            gpio_hal_write(a1_gpio, true);
        break;
    }

    sys_hal_sleep_us(16);

    if(_tmux1204_read_functions[driver_instance])
    {
        return _tmux1204_read_functions[driver_instance](&_tmux1204_nested_channels[driver_instance]);
    }

    return 0;
}
