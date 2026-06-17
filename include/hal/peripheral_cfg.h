#ifndef HAL_PERIPHERAL_CFG_H
#define HAL_PERIPHERAL_CFG_H

#include "hoja_bsp.h"

#include <stdbool.h>
#include <stdint.h>

#define HOJA_I2C_DEFAULT_BAUDRATE_KHZ 400

#if defined(HOJA_BSP_HAS_I2C) && (HOJA_BSP_HAS_I2C > 0)

typedef struct
{
    bool     enabled;
    uint8_t  sda_gpio;
    uint8_t  scl_gpio;
    uint16_t baudrate_khz; // 0 uses HOJA_I2C_DEFAULT_BAUDRATE_KHZ
} hoja_i2c_instance_cfg_s;

// Named per hardware instance (count follows HOJA_BSP_HAS_I2C) so board main.c
// can use .instance_0 / .instance_1 designated initializers without array indices.
typedef struct
{
#if (HOJA_BSP_HAS_I2C >= 1)
    hoja_i2c_instance_cfg_s instance_0;
#endif
#if (HOJA_BSP_HAS_I2C >= 2)
    hoja_i2c_instance_cfg_s instance_1;
#endif
#if (HOJA_BSP_HAS_I2C >= 3)
    hoja_i2c_instance_cfg_s instance_2;
#endif
#if (HOJA_BSP_HAS_I2C >= 4)
    hoja_i2c_instance_cfg_s instance_3;
#endif
} hoja_i2c_cfg_s;

#endif

#if defined(HOJA_BSP_HAS_SPI) && (HOJA_BSP_HAS_SPI > 0)

typedef struct
{
    bool     enabled;
    uint8_t  clk_gpio;
    uint8_t  mosi_gpio;
    uint8_t  miso_gpio;
} hoja_spi_instance_cfg_s;

typedef struct
{
#if (HOJA_BSP_HAS_SPI >= 1)
    hoja_spi_instance_cfg_s instance_0;
#endif
#if (HOJA_BSP_HAS_SPI >= 2)
    hoja_spi_instance_cfg_s instance_1;
#endif
#if (HOJA_BSP_HAS_SPI >= 3)
    hoja_spi_instance_cfg_s instance_2;
#endif
#if (HOJA_BSP_HAS_SPI >= 4)
    hoja_spi_instance_cfg_s instance_3;
#endif
} hoja_spi_cfg_s;

#endif

#endif
