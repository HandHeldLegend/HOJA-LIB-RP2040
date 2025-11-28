#include "drivers/adc/mcp3002.h"
#include "board_config.h"
#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"

#define BUFFER_TO_UINT16(buffer) (uint16_t)(((buffer[0] & 0x07) << 9) | buffer[1] << 1 | buffer[2] >> 7)
#define CH0_CONFIG 0xD0
#define CH1_CONFIG 0xF0

// Register MCP3002 driver
bool mcp3002_init(adc_mcp3002_driver_s *driver)
{
    if(driver->initialized) return false;

    // Init this GPIO
    gpio_hal_init(driver->cs_gpio, true, false);
    // Set GPIO to true
    gpio_hal_write(driver->cs_gpio, true);
    return true;
}

bool mcp3002_read(adc_mcp3002_driver_s *driver)
{
    uint8_t buffer[3] = {0};
    uint8_t spi_instance    = driver->spi_instance;
    uint8_t cs_gpio         = driver->cs_gpio;

    spi_hal_read_blocking(spi_instance, cs_gpio, CH0_CONFIG, buffer, 3);
    driver->output_ch_0 = BUFFER_TO_UINT16(buffer);
    spi_hal_read_blocking(spi_instance, cs_gpio, CH1_CONFIG, buffer, 3);
    driver->output_ch_1 = BUFFER_TO_UINT16(buffer);

    return true;
}
