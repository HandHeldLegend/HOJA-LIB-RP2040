#include "drivers/adc/tmux1204.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"

#include "drivers/adc/mcp3002.h"
#include "hal/adc_hal.h"

typedef struct 
{
    adc_read_fn_t read_fn;
    int8_t        ch_local;
    int8_t        driver_instance;
} tmux1204_driver_fwd_s;

adc_tmux1204_cfg_t  _tmux1204_drivers[ADC_CH_MAX] = {0};
bool                _tmux1204_initialized[ADC_CH_MAX] = {0};

// Drivers that this MUX uses
tmux1204_driver_fwd_s _tmux1204_fwds[ADC_CH_MAX];

bool     tmux1204_register_driver(uint8_t ch_local, adc_driver_cfg_s *cfg)
{
    if(cfg->driver_instance<0) return false;

    int8_t instance = cfg->driver_instance;
    adc_tmux1204_cfg_t *driver = &_tmux1204_drivers[instance];
    adc_tmux1204_cfg_t *read    = &cfg->tmux1204_cfg;

    // Only initialize one of each driver instance
    if(_tmux1204_initialized[instance]) return true;

    adc_driver_cfg_s *fwd_cfg = read->host_cfg;

    // Initialize this base driver for this TMUX chip
    switch(fwd_cfg->driver_type)
    {
        default:
        return false;

        case ADC_DRIVER_HAL:
        if(adc_hal_register_driver(driver->host_ch_local, fwd_cfg))
        {
            _tmux1204_fwds[instance].read_fn            = adc_hal_read;
            _tmux1204_fwds[instance].ch_local           = read->host_ch_local;
            _tmux1204_fwds[instance].driver_instance    = instance;
            return true;
        }  
        break;

        case ADC_DRIVER_MCP3002:
        if(mcp3002_register_driver(driver->host_ch_local, fwd_cfg))
        {
            _tmux1204_fwds[instance].read_fn            = mcp3002_read;
            _tmux1204_fwds[instance].ch_local           = read->host_ch_local;
            _tmux1204_fwds[instance].driver_instance    = instance;
            return true;
        }  
        break;
    }

    return false;
}

uint16_t    tmux1204_read(uint8_t ch_local, uint8_t driver_instance)
{
    if(ch_local > 3) return 0;

    adc_tmux1204_cfg_t *cfg = &_tmux1204_drivers[driver_instance];

    switch(ch_local)
    {
        case 0:
            gpio_hal_write(cfg->a1_gpio, false);
            gpio_hal_write(cfg->a2_gpio, false);
        break;

        case 1:
            gpio_hal_write(cfg->a1_gpio, false);
            gpio_hal_write(cfg->a2_gpio, false);
        break;

        case 2:
            gpio_hal_write(cfg->a1_gpio, false);
            gpio_hal_write(cfg->a2_gpio, false);
        break;

        case 3:
            gpio_hal_write(cfg->a1_gpio, false);
            gpio_hal_write(cfg->a2_gpio, false);
        break;
    }

    int8_t ch = _tmux1204_fwds[driver_instance].ch_local;
    int8_t dr = _tmux1204_fwds[driver_instance].driver_instance;

    // Read from the appropriate lower level driver
    if(_tmux1204_fwds[driver_instance].read_fn)
        return _tmux1204_fwds[driver_instance].read_fn(ch, dr);
    else 
        return 0;
}
