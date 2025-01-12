#include "hal/spi_hal.h"
#include "board_config.h"

// Pico SDK specific code
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/multicore.h"

#define SPI_HAL_MAX_INSTANCES 2

// SPI
#if defined(HOJA_SPI_0_ENABLE) && (HOJA_SPI_0_ENABLE==1)
    #ifndef HOJA_SPI_0_GPIO_CLK
      #error "HOJA_SPI_0_GPIO_CLK undefined in board_config.h"
    #endif

    #ifndef HOJA_SPI_0_GPIO_MISO
      #error "HOJA_SPI_0_GPIO_MISO undefined in board_config.h"
    #endif

    #ifndef HOJA_SPI_0_GPIO_MOSI
      #error "HOJA_SPI_0_GPIO_MOSI undefined in board_config.h"
    #endif
#endif

spi_inst_t* _spi_instances[2] = {spi0, spi1}; // Numerical accessible array to spi hardware
auto_init_mutex(_spi_safe_mutex); // Mutex to allow thread-safe access to peripheral

void _spi_safe_enter_blocking()
{
    //mutex_enter_blocking(&_spi_safe_mutex);
}

void _spi_safe_exit()
{
    //mutex_exit(&_spi_safe_mutex);
}

bool spi_hal_init(uint8_t instance, uint32_t clock, uint32_t mosi, uint32_t miso)
{
    if(instance>=SPI_HAL_MAX_INSTANCES) return false;

    spi_init(_spi_instances[instance], 3000 * 1000);
    gpio_set_function(clock,    GPIO_FUNC_SPI);
    gpio_set_function(mosi,     GPIO_FUNC_SPI);
    gpio_set_function(miso,     GPIO_FUNC_SPI);

    return true;
}

bool spi_hal_deinit(uint8_t instance)
{

}

int spi_hal_read_blocking(uint8_t instance, uint32_t cs_gpio, uint8_t repeated_tx_data, uint8_t *dst, size_t len)
{
    if(instance>=SPI_HAL_MAX_INSTANCES) return -1;

    int ret = 0;

    _spi_safe_enter_blocking();
    gpio_put(cs_gpio, false);

    ret = spi_read_blocking(_spi_instances[instance], repeated_tx_data, dst, len);

    gpio_put(cs_gpio, true);
    _spi_safe_exit();

    return ret;
}

int spi_hal_write_blocking(uint8_t instance, uint32_t cs_gpio, const uint8_t *data, size_t len)
{
    if(instance>=SPI_HAL_MAX_INSTANCES) return -1;

    int ret = 0;
    _spi_safe_enter_blocking();
    gpio_put(cs_gpio, false);

    ret = spi_write_blocking(_spi_instances[instance], data, len);

    gpio_put(cs_gpio, true);
    _spi_safe_exit();

    return ret;
}

int spi_hal_read_write_blocking(uint8_t instance, uint32_t cs_gpio, const uint8_t *data, size_t data_len, uint8_t repeated_tx_data, uint8_t *dst, size_t dst_len)
{
    if(instance>=SPI_HAL_MAX_INSTANCES) return -1;

    int ret = 0;
    _spi_safe_enter_blocking();
    gpio_put(cs_gpio, false);

    ret = spi_write_blocking(_spi_instances[instance],  data, data_len);
    ret = spi_read_blocking(_spi_instances[instance],   repeated_tx_data, dst, dst_len);

    gpio_put(cs_gpio, true);
    _spi_safe_exit();
    return ret;
}
