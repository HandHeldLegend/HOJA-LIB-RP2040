#include "hal/hal.h"
#include "hal/sys_hal.h"
#include "board_config.h"

bool hal_init()
{
    sys_hal_init();

    // SPI 0
    #if defined(HOJA_SPI_0_ENABLE) && (HOJA_SPI_0_ENABLE==1)
    spi_hal_init(0, HOJA_SPI_0_GPIO_CLK, HOJA_SPI_0_GPIO_MISO, HOJA_SPI_0_GPIO_MOSI);
    #endif

    // I2C 0
    #if defined(HOJA_I2C_0_ENABLE) && (HOJA_I2C_0_ENABLE==1)
    i2c_hal_init(0, HOJA_I2C_0_GPIO_SDA, HOJA_I2C_0_GPIO_SCL);
    #endif

    // HD Rumble
    #if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
    //hdrumble_hal_init();
    #endif
}