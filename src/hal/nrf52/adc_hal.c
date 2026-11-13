#include "hal/adc_hal.h"

#include "nrf52_hal.h"

// adc_hal_read's fixed settings: the whole supply range (gain 1/4 against VDD/4), a 10 us
// acquisition, eight samples averaged as the RP2040 HAL does
#define ADC_HAL_DEFAULT_SAMPLES 8u
#define ADC_HAL_SAMPLE_WAIT_SPINS 100000u

// AIN0-AIN7 are fixed pins on port 0
static const uint8_t _adc_hal_ain_gpio[8] = {2, 3, 4, 5, 28, 29, 30, 31};

static bool _adc_hal_enabled = false;

static void _adc_hal_enable(void)
{
    if (_adc_hal_enabled)
        return;
    _adc_hal_enabled = true;

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;
    NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;
    NRF_SAADC->SAMPLERATE = SAADC_SAMPLERATE_MODE_Task << SAADC_SAMPLERATE_MODE_Pos;
    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

    // The offset calibration the chip wants once after power-up
    NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
    NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    for (uint32_t i = 0; (i < ADC_HAL_SAMPLE_WAIT_SPINS) && !NRF_SAADC->EVENTS_CALIBRATEDONE; i++)
    {
    }
    NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
}

uint8_t nrf52_adc_gpio_to_ain(uint32_t gpio)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (_adc_hal_ain_gpio[i] == gpio)
            return i;
    }
    return 0xFF;
}

static bool _adc_hal_wait(volatile uint32_t *event)
{
    for (uint32_t i = 0; i < ADC_HAL_SAMPLE_WAIT_SPINS; i++)
    {
        if (*event)
        {
            *event = 0;
            return true;
        }
    }
    return false;
}

// One scan: every channel in turn, each converted `samples` times back to back and averaged by the
// SAADC itself (burst oversampling), so the CPU only waits for the end.
void nrf52_adc_read(const nrf52_adc_channel_s *channels, uint8_t count, uint8_t samples, uint16_t *out)
{
    static volatile int16_t results[NRF52_ADC_CHANNELS_MAX];

    if (count > NRF52_ADC_CHANNELS_MAX)
        count = NRF52_ADC_CHANNELS_MAX;
    _adc_hal_enable();

    // The oversampling is a power of two: the largest that `samples` covers
    uint32_t oversample = 0;
    while ((oversample < SAADC_OVERSAMPLE_OVERSAMPLE_Over256x) && ((2u << oversample) <= samples))
        oversample++;
    NRF_SAADC->OVERSAMPLE = oversample;

    for (uint8_t c = 0; c < NRF52_ADC_CHANNELS_MAX; c++)
    {
        if (c >= count)
        {
            NRF_SAADC->CH[c].PSELP = SAADC_CH_PSELP_PSELP_NC;
            continue;
        }
        NRF_SAADC->CH[c].PSELP = (uint32_t)channels[c].ain + SAADC_CH_PSELP_PSELP_AnalogInput0;
        NRF_SAADC->CH[c].PSELN = SAADC_CH_PSELN_PSELN_NC;
        NRF_SAADC->CH[c].CONFIG =
            ((uint32_t)channels[c].gain << SAADC_CH_CONFIG_GAIN_Pos) |
            ((uint32_t)channels[c].reference << SAADC_CH_CONFIG_REFSEL_Pos) |
            ((uint32_t)channels[c].acquisition << SAADC_CH_CONFIG_TACQ_Pos) |
            (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) |
            ((oversample ? SAADC_CH_CONFIG_BURST_Enabled : SAADC_CH_CONFIG_BURST_Disabled)
             << SAADC_CH_CONFIG_BURST_Pos);
    }

    NRF_SAADC->RESULT.PTR = (uint32_t)results;
    NRF_SAADC->RESULT.MAXCNT = count;

    NRF_SAADC->EVENTS_STARTED = 0;
    NRF_SAADC->EVENTS_END = 0;
    NRF_SAADC->TASKS_START = 1;
    _adc_hal_wait(&NRF_SAADC->EVENTS_STARTED);
    NRF_SAADC->TASKS_SAMPLE = 1;
    const bool done = _adc_hal_wait(&NRF_SAADC->EVENTS_END);

    NRF_SAADC->EVENTS_STOPPED = 0;
    NRF_SAADC->TASKS_STOP = 1;
    _adc_hal_wait(&NRF_SAADC->EVENTS_STOPPED);

    for (uint8_t c = 0; c < count; c++)
    {
        // Single-ended readings dip a few counts below zero at the bottom of the range
        int32_t value = done ? results[c] : 0;
        if (value < 0)
            value = 0;
        out[c] = (uint16_t)((value > 4095) ? 4095 : value);
    }
}

bool adc_hal_read(adc_hal_driver_s *driver)
{
    if (!driver->initialized)
        return false;

    const nrf52_adc_channel_s channel = {
        .ain = driver->ch,
        .gain = SAADC_CH_CONFIG_GAIN_Gain1_4,
        .reference = SAADC_CH_CONFIG_REFSEL_VDD1_4,
        .acquisition = SAADC_CH_CONFIG_TACQ_10us,
    };
    nrf52_adc_read(&channel, 1, ADC_HAL_DEFAULT_SAMPLES, &driver->output);
    return true;
}

bool adc_hal_init(adc_hal_driver_s *driver)
{
    const uint8_t ain = nrf52_adc_gpio_to_ain(driver->gpio);
    if (ain == 0xFF)
        return false;

    _adc_hal_enable();
    driver->ch = ain;
    driver->initialized = true;
    return true;
}
