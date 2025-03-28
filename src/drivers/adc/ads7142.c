#include "drivers/adc/ads7142.h"
#include "board_config.h"
#include "hal/i2c_hal.h"

bool _ads7142_initialized[ADC_CH_MAX] = {0};
#define ADS7142_ADDR 0x18

typedef struct {
    uint16_t a_x;
    uint16_t a_y;
} ads7142_reading_t;

ads7142_reading_t _ads7142_readings[ADC_CH_MAX] = {0};

bool _ads7142_write_register(uint8_t i2c_instance, uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t data[] = {0x8, reg, value};

    int ret = i2c_hal_write_timeout_us(i2c_instance, addr, data, 3, false, 1000);
    if(ret<0) return false;
}

// Register ads7142 driver
bool ads7142_init_channel(adc_channel_cfg_s *cfg)
{
    if(cfg->ch_local > 1) return false;
    // Get associated driver
    adc_driver_cfg_s *driver   = cfg->driver_cfg;

    uint8_t driver_instance = driver->driver_instance;

    // Max 7 instances
    if(driver_instance>7) return false;

    // If we already initialized this channel CS pin, return
    if(_ads7142_initialized[driver_instance]) return true;

    uint8_t i2c_instance = driver->ads7142_cfg.i2c_instance;

    bool ret = _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x1F, 0x01);
    if(!ret) return false;
    
    // OFFSET_CAL
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x15, 0x01);

    // INPUT CONFIG REGISTER
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x24, 0x03);

    // OPMODE_SEL
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x1C, 0x07);

    // AUTO_SEQ_CHEN 
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x20, 0x03);

    // OSC_SEL 
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x18, 0x00);

    // nCLK_SEL
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x19, 0x15);

    // ACC_EN 
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x30, 0x0F);

    // START_SEQUENCE 
    _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x1E, 0x01);

    _ads7142_initialized[driver_instance] = true;

    return true;
}

uint16_t ads7142_read_channel(adc_channel_cfg_s *cfg)
{
    uint8_t driver_instance = cfg->driver_cfg->driver_instance;
    if(!_ads7142_initialized[driver_instance]) return 0;

    uint16_t return_value = 0;

    // For channel 1, return the last reading
    if(cfg->ch_local >= 1) 
    {
        return_value = _ads7142_readings[cfg->driver_cfg->driver_instance].a_y;
    }
    else
    {
        uint8_t data_buf[4] = {
            0x30,
            0x08,
            0,
            0,
        };

        uint8_t i2c_instance = cfg->driver_cfg->ads7142_cfg.i2c_instance;

        i2c_hal_write_read_blocking(i2c_instance, ADS7142_ADDR, data_buf, 2, data_buf, 4);

        _ads7142_readings[cfg->driver_cfg->driver_instance].a_x = ((data_buf[3] << 8) + data_buf[2]);
        return_value = _ads7142_readings[cfg->driver_cfg->driver_instance].a_x;

        _ads7142_readings[cfg->driver_cfg->driver_instance].a_y = ((data_buf[1] << 8) + data_buf[0]);

        // START_SEQUENCE
        _ads7142_write_register(i2c_instance, ADS7142_ADDR, 0x1E, 0x01);
    }
    
    bool ch_invert = cfg->ch_invert;

    return (ch_invert) ? (0xFFF -  return_value) : return_value;
}
