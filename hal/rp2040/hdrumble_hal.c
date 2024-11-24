#include "hal/hdrumble_hal.h"
#include "pico/stdlib.h"

#ifdef CONFIG_HAL_RUMBLE_EN
#if (CONFIG_HAL_RUMBLE_EN == 1)
#if (CONFIG_HAL_RUMBLE_TYPE == CONFIG_HAL_TYPE_HDRUMBLE)

// We are in Pico land, use native APIs here :)

#define SIN_TABLE_SIZE 4096
int16_t sin_table[SIN_TABLE_SIZE] = {0};
#define SIN_RANGE_MAX 128
#define M_PI 3.14159265358979323846
#define TWO_PI (M_PI * 2)

void _sine_table_init()
{
    float inc = TWO_PI / SIN_TABLE_SIZE;
    float fi = 0;

    for (int i = 0; i < SIN_TABLE_SIZE; i++)
    {
        float sample = sinf(fi);

        sin_table[i] = (int16_t)(sample * SIN_RANGE_MAX);

        fi += inc;
        fi = fmodf(fi, TWO_PI);
    }
}

bool hdrumble_hal_init()
{
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 254.0f / (float)sample_freq / (float)REPETITION_RATE;

    #ifdef CONFIG_HDRUMBLE_GPIO_L

    gpio_hal_init(CONFIG_HDRUMBLE_GPIO_L)

    #endif

    #ifdef CONFIG_HDRUMBLE_GPIO_R
    #endif
}

#endif
#endif
#endif