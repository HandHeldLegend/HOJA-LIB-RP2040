#include "devices/adc.h"
#include "driver_define_helper.h"
#include "board_config.h"
#include <stdlib.h>
#include "utilities/settings.h"

#include "drivers/adc/mcp3002.h"
#include "drivers/adc/tmux1204.h"
#include "hal/adc_hal.h"

#define BLANK_ADC_CFG {.ch_local = -1}

#if !defined(HOJA_ADC_LX_CFG)
    #define HOJA_ADC_LX_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_LY_CFG)
    #define HOJA_ADC_LY_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_RX_CFG)
    #define HOJA_ADC_RX_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_RY_CFG)
    #define HOJA_ADC_RY_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_LT_CFG)
    #define HOJA_ADC_LT_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_RT_CFG)
    #define HOJA_ADC_RT_CFG BLANK_ADC_CFG
#endif 

#if !defined(HOJA_ADC_BAT_CFG)
    #define HOJA_ADC_BAT_CFG BLANK_ADC_CFG
#endif 

adc_channel_cfg_s _chan_cfgs[ADC_CH_MAX] = {
    HOJA_ADC_LX_CFG, HOJA_ADC_LY_CFG, 
    HOJA_ADC_RX_CFG, HOJA_ADC_RY_CFG,
    HOJA_ADC_LT_CFG, HOJA_ADC_RT_CFG,
    HOJA_ADC_BAT_CFG
    };

adc_read_fn_t _chan_read_fns[ADC_CH_MAX] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

bool adc_devices_init()
{
    for(int i = 0; i < 7; i++)
    {
        int8_t          driver_num = -1;
        adc_read_fn_t   read_fn = NULL;

        adc_channel_cfg_s *cfg = &_chan_cfgs[i];

        // If our channel config exists
        if(cfg->ch_local > -1)
        {
            adc_driver_cfg_s *driver_cfg = cfg->driver_cfg;

            switch(driver_cfg->driver_type)
            {
                case ADC_DRIVER_NONE:
                break;

                case ADC_DRIVER_MCP3002:
                    if(mcp3002_init_channel(cfg))
                        _chan_read_fns[i] = mcp3002_read_channel;
                break;

                case ADC_DRIVER_HAL:
                    if(adc_hal_init_channel(cfg))
                        _chan_read_fns[i] = adc_hal_read_channel;
                break;

                case ADC_DRIVER_TMUX1204:
                    if(tmux1204_init_channel(cfg))
                        _chan_read_fns[i] = tmux1204_read_channel;
                break;
            }
        }
    }

    return true;
}

static inline int _read_adc_instance(uint8_t idx)
{
    if(_chan_read_fns[idx])
    {
        adc_channel_cfg_s *cfg = &_chan_cfgs[idx];
        return _chan_read_fns[idx](cfg);
    }
        
    return -1;
}

uint16_t adc_read_lx()
{
    int v = _read_adc_instance(0);
    return (v > -1) ? v : 2048;
}

uint16_t adc_read_ly()
{
    int v = _read_adc_instance(1);
    return (v > -1) ? v : 2048;
}

uint16_t adc_read_rx()
{
    int v = _read_adc_instance(2);
    return (v > -1) ? v : 2048;
}

uint16_t adc_read_ry()
{
    int v = _read_adc_instance(3);
    return (v > -1) ? v : 2048;
}

uint16_t adc_read_lt()
{
    int v = _read_adc_instance(4);
    return (v > -1) ? v : 0;
}

uint16_t adc_read_rt()
{
    int v = _read_adc_instance(5);
    return (v > -1) ? v : 0;
}

uint16_t adc_read_battery()
{
    int v = _read_adc_instance(6);
    return (v > -1) ? v : 0;
}