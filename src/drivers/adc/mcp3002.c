#include "drivers/adc/mcp3002.h"
#include "board_config.h"
#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"

#ifdef ADC_DRIVER_MCP3002
#if (ADC_DRIVER_MCP3002 > 0)

#define CH0_CONFIG 0xD0
#define CH1_CONFIG 0xF0
#define BUFFER_TO_UINT16(buffer) (uint16_t)(((buffer[0] & 0x07) << 9) | buffer[1] << 1 | buffer[2] >> 7)

uint16_t mcp3002_read(bool channel, uint32_t cs_gpio, uint8_t spi_instance)
{

    uint8_t buffer[3] = {0};

    if(channel)
    {
        spi_hal_read_blocking(spi_instance, cs_gpio, CH1_CONFIG, buffer, 3);
    }
    else
    {
        spi_hal_read_blocking(spi_instance, cs_gpio, CH0_CONFIG, buffer, 3);
    }

    return BUFFER_TO_UINT16(buffer);
}

bool mcp3002_init(uint32_t cs_gpio)
{
    gpio_hal_init(cs_gpio, true, false);
    return true;
}

#endif
#endif
