#ifndef HAL_NRF52_NRF52_HAL_H
#define HAL_NRF52_NRF52_HAL_H

// What the nRF52 HAL offers beyond the portable hal/*.h headers, for board code (main.c) and the
// HAL's own files.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf.h"

#include "hoja_bsp.h"

// ---- GPIO ----

static inline NRF_GPIO_Type *nrf52_gpio_port(uint32_t gpio)
{
    return (gpio < 32u) ? NRF_P0 : NRF_P1;
}

static inline uint32_t nrf52_gpio_bit(uint32_t gpio)
{
    return 1u << (gpio & 31u);
}

// The value of a peripheral's PSEL register for a GPIO (connected)
static inline uint32_t nrf52_psel(uint32_t gpio)
{
    return gpio & 0x3Fu;
}

#define NRF52_PSEL_DISCONNECTED 0xFFFFFFFFu

// Input with a pull-down, or with no pull at all (gpio_hal_init offers pull-up or none)
void nrf52_gpio_init_input(uint32_t gpio, bool pull_up, bool pull_down);

// ---- Records ----

// The record store's newest copies, into the RAM a board keeps for them over an update that erases
// the store (record_hal.c). sys_hal_bootloader does this; the next start puts them back.
void nrf52_record_carry(void);

// ---- I2C ----

// A read that runs by itself (the unit's DMA) while the task loop carries on: for a long read made
// often, which would otherwise hold the loop for its whole length (2 ms for 80 bytes at 400 kHz).
// One at a time for an instance. Any other call on the instance first waits for it to end, so a
// caller that shares the bus asks nrf52_i2c_read_busy before a call it would rather skip.
bool nrf52_i2c_read_start(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len);
bool nrf52_i2c_read_busy(uint8_t instance);
// Once it is no longer busy: the bytes read, or a negative error. 0 when none was started.
int nrf52_i2c_read_result(uint8_t instance);

// ---- System ----

// System OFF, the chip's deepest sleep: a low level on wake_gpio (pulled up) starts it again as a
// reset. Never returns.
void nrf52_system_off(uint32_t wake_gpio);

// True while USB power is on the chip's VBUS pin
bool nrf52_usb_power_present(void);

// Why the chip last reset (POWER RESETREAS, read and cleared at boot)
uint32_t nrf52_reset_reason(void);

// One alarm on the microsecond clock: fn runs from the clock's interrupt at at_us, or at once if that
// has passed. Setting it again replaces the one waiting.
void nrf52_alarm_set(uint64_t at_us, void (*fn)(void));
void nrf52_alarm_cancel(void);

// The board's way into its bootloader, called by sys_hal_bootloader just before the reset. The HAL's
// default does nothing (a plain restart).
void cb_nrf52_bootloader_request(void);

// ---- SAADC ----

// One analog input as the SAADC reads it. gain, reference and acquisition are the SAADC_CH_CONFIG
// field values (SAADC_CH_CONFIG_GAIN_Gain1_4 and so on).
typedef struct
{
    uint8_t ain;         // 0-7 for AIN0-AIN7
    uint8_t gain;
    uint8_t reference;
    uint8_t acquisition;
} nrf52_adc_channel_s;

// The SAADC input a GPIO is, or 0xFF
uint8_t nrf52_adc_gpio_to_ain(uint32_t gpio);

#define NRF52_ADC_CHANNELS_MAX 8u

// Reads up to eight channels in one scan (12 bits), each `samples` times (a power of two) with the
// mean given, 0-4095. Blocking: 2 us and the acquisition time for every sample.
void nrf52_adc_read(const nrf52_adc_channel_s *channels, uint8_t count, uint8_t samples, uint16_t *out);

#endif
