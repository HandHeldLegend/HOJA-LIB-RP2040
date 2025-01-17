#include "devices/adc.h"
#include "driver_define_helper.h"
#include "board_config.h"
#include <stdlib.h>

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

const adc_channel_cfg_s _chan_cfgs[7] = {
    HOJA_ADC_LX_CFG, HOJA_ADC_LY_CFG, 
    HOJA_ADC_RX_CFG, HOJA_ADC_RY_CFG,
    HOJA_ADC_LT_CFG, HOJA_ADC_RT_CFG,
    HOJA_ADC_BAT_CFG
    };

#define LNULL {.driver_instance = -1, .ch_local = -1}
adc_local_read_s _adc_channels[7] = {LNULL,LNULL,LNULL,LNULL,LNULL,LNULL,LNULL};

bool adc_devices_init()
{
    for(int i = 0; i < 7; i++)
    {
        int8_t          driver_num = -1;
        adc_read_fn_t   read_fn = NULL;

        // If our channel config exists
        if(_chan_cfgs[i].ch_local > -1)
        {
            adc_driver_cfg_s    *driver_cfg = _chan_cfgs[i].driver_cfg;
            uint8_t ch_local = _chan_cfgs[i].ch_local;

            switch(driver_cfg->driver_type)
            {
                case ADC_DRIVER_NONE:
                break;

                case ADC_DRIVER_MCP3002:
                    if(mcp3002_register_driver(ch_local, driver_cfg))
                        read_fn = mcp3002_read;
                break;

                case ADC_DRIVER_HAL:
                    if(adc_hal_register_driver(ch_local, driver_cfg))
                        read_fn = adc_hal_read;
                break;

                case ADC_DRIVER_TMUX1204:
                    if(tmux1204_register_driver(ch_local, driver_cfg))
                        read_fn = tmux1204_read;
                break;
            }

            if(read_fn)
            {
                _adc_channels[i].ch_local           = _chan_cfgs[i].ch_local;
                _adc_channels[i].driver_instance    = _chan_cfgs[i].driver_cfg->driver_instance;
                _adc_channels[i].read_fn            = read_fn;
            }
        }
    }

    return true;
}

static inline int _read_adc_instance(uint8_t idx)
{
    if(_adc_channels[idx].driver_instance > -1)
        return _adc_channels[idx].read_fn(_adc_channels[idx].ch_local, _adc_channels[idx].driver_instance);
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