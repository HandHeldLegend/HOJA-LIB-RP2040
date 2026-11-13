#include "hal/sys_hal.h"

#include <stddef.h>
#include <stdint.h>

#include "nrf52_hal.h"

// The microsecond clock: TIMER2 counting at 1 MHz, 32 bits wide, its wraps counted in the interrupt
#define SYS_HAL_TIMER           NRF_TIMER2
#define SYS_HAL_TIMER_IRQ       TIMER2_IRQn
#define SYS_HAL_TIMER_PRESCALER 4u // 16 MHz / 2^4
#define SYS_HAL_CC_READ         0u
#define SYS_HAL_CC_WRAP         1u
#define SYS_HAL_CC_ALARM        2u

#define SYS_HAL_WDT_TIMEOUT_S   5u
#define SYS_HAL_WDT_RELOAD      0x6E524635u
#define SYS_HAL_HFXO_START_US   5000u

// The mode carried over a restart. The RP2040 keeps it in a watchdog scratch register; here it is RAM
// the reset leaves alone (.noinit), with its complement beside it so stale RAM reads as nothing.
// A board whose restarts pass through a bootloader that uses RAM gives two words that bootloader
// leaves alone instead (its linker script's __boot_memory).
static volatile uint32_t _sys_hal_bootmemory_own[2] __attribute__((section(".noinit")));
extern uint32_t __boot_memory[2] __attribute__((weak));

static volatile uint32_t *_sys_hal_bootmemory_at(void)
{
    return __boot_memory ? (volatile uint32_t *)__boot_memory : _sys_hal_bootmemory_own;
}

static volatile uint32_t _sys_hal_time_high = 0;
static void (*volatile _sys_hal_alarm_fn)(void) = NULL;
static volatile uint64_t _sys_hal_alarm_at_us = 0;
static uint32_t _sys_hal_reset_reason = 0;

extern void _start(void);

// The reset code's hand-over to the C library (startup_nrf52833.S), interrupts still masked. A
// bootloader in front of the firmware may have left peripherals running with their interrupts
// switched on: with the interrupt lines themselves now disabled they are harmless, until we enable a
// line for our own use and inherit an event nothing here handles. So everything the HAL shares a line
// with, or that could act by itself, is put back to rest first. The watchdog is left: it can't be
// stopped, and sys_hal_tick feeds it.
void sys_hal_nrf52_start(void)
{
    NRF_PPI->CHENCLR = 0xFFFFFFFFu;

    NRF_POWER->INTENCLR = 0xFFFFFFFFu;
    NRF_CLOCK->INTENCLR = 0xFFFFFFFFu;
    NRF_GPIOTE->INTENCLR = 0xFFFFFFFFu;
    NRF_RADIO->INTENCLR = 0xFFFFFFFFu;

    NRF_TIMER_Type *const timers[] = {NRF_TIMER0, NRF_TIMER1, NRF_TIMER2, NRF_TIMER3, NRF_TIMER4};
    for (uint32_t i = 0; i < (sizeof(timers) / sizeof(timers[0])); i++)
    {
        timers[i]->TASKS_STOP = 1;
        timers[i]->INTENCLR = 0xFFFFFFFFu;
        timers[i]->SHORTS = 0;
    }

    NRF_RTC_Type *const rtcs[] = {NRF_RTC0, NRF_RTC1, NRF_RTC2};
    for (uint32_t i = 0; i < (sizeof(rtcs) / sizeof(rtcs[0])); i++)
    {
        rtcs[i]->TASKS_STOP = 1;
        rtcs[i]->INTENCLR = 0xFFFFFFFFu;
        rtcs[i]->EVTENCLR = 0xFFFFFFFFu;
    }

    // USB off the bus and powered down: the USB stack brings it up from nothing
    if (NRF_USBD->ENABLE)
    {
        NRF_USBD->USBPULLUP = 0;
        NRF_USBD->INTENCLR = 0xFFFFFFFFu;
        NRF_USBD->ENABLE = 0;
    }

    // The units the HAL configures itself, off until it does
    NRF_TWIM0->ENABLE = 0;
    NRF_TWIM1->ENABLE = 0;
    NRF_SPIM2->ENABLE = 0;
    NRF_SPIM3->ENABLE = 0;
    NRF_UARTE0->ENABLE = 0;
    NRF_UARTE1->ENABLE = 0;
    NRF_SAADC->INTENCLR = 0xFFFFFFFFu;
    NRF_SAADC->ENABLE = 0;
    NRF_PWM0->ENABLE = 0;
    NRF_PWM1->ENABLE = 0;
    NRF_I2S->ENABLE = 0;
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;

    __enable_irq();
    _start();
}

void TIMER2_IRQHandler(void)
{
    if (SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_WRAP])
    {
        SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_WRAP] = 0;
        (void)SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_WRAP];
        _sys_hal_time_high++;
    }

    // The compare matches the low half of the alarm's time, so it also fires a whole wrap early for
    // one more than 71 minutes off: only the clock says whether it is due
    SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_ALARM] = 0;
    void (*fn)(void) = _sys_hal_alarm_fn;
    if (fn && (sys_hal_now_us() >= _sys_hal_alarm_at_us))
    {
        _sys_hal_alarm_fn = NULL;
        SYS_HAL_TIMER->INTENCLR = TIMER_INTENCLR_COMPARE2_Msk;
        fn();
    }
}

void nrf52_alarm_set(uint64_t at_us, void (*fn)(void))
{
    const uint32_t state = sys_hal_irq_save();

    _sys_hal_alarm_at_us = at_us;
    _sys_hal_alarm_fn = fn;
    SYS_HAL_TIMER->CC[SYS_HAL_CC_ALARM] = (uint32_t)at_us;
    SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_ALARM] = 0;
    SYS_HAL_TIMER->INTENSET = TIMER_INTENSET_COMPARE2_Msk;
    // Due already, or too close for the compare to be sure of catching it
    if (sys_hal_now_us() + 4u >= at_us)
        NVIC_SetPendingIRQ(SYS_HAL_TIMER_IRQ);

    sys_hal_irq_restore(state);
}

void nrf52_alarm_cancel(void)
{
    const uint32_t state = sys_hal_irq_save();
    _sys_hal_alarm_fn = NULL;
    SYS_HAL_TIMER->INTENCLR = TIMER_INTENCLR_COMPARE2_Msk;
    sys_hal_irq_restore(state);
}

uint64_t sys_hal_now_us(void)
{
    const uint32_t masked = __get_PRIMASK();
    __disable_irq();

    SYS_HAL_TIMER->TASKS_CAPTURE[SYS_HAL_CC_READ] = 1;
    const uint32_t low = SYS_HAL_TIMER->CC[SYS_HAL_CC_READ];
    uint32_t high = _sys_hal_time_high;
    // Wrapped, and its interrupt is still waiting on us
    if (SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_WRAP] && (low < 0x80000000u))
        high++;

    if (!masked)
        __enable_irq();

    return ((uint64_t)high << 32) | low;
}

void sys_hal_time_us(uint64_t *out)
{
    if (out)
        *out = sys_hal_now_us();
}

void sys_hal_time_ms(uint64_t *out)
{
    if (out)
        *out = sys_hal_now_us() / 1000u;
}

void sys_hal_sleep_us(uint32_t us)
{
    const uint64_t until = sys_hal_now_us() + us;
    while (sys_hal_now_us() < until)
    {
    }
}

void sys_hal_sleep_ms(uint32_t ms)
{
    const uint64_t until = sys_hal_now_us() + (uint64_t)ms * 1000u;
    while (sys_hal_now_us() < until)
    {
    }
}

void sys_hal_reboot()
{
    NVIC_SystemReset();
    for (;;)
    {
    }
}

__attribute__((weak)) void cb_nrf52_bootloader_request(void)
{
}

void sys_hal_bootloader()
{
    // The update may erase what the board has saved
    nrf52_record_carry();
    cb_nrf52_bootloader_request();
    sys_hal_reboot();
}

void sys_hal_rom_bootloader(void)
{
    sys_hal_bootloader();
}

void sys_hal_tick(uint64_t now_us)
{
    (void)now_us;
    NRF_WDT->RR[0] = SYS_HAL_WDT_RELOAD;
}

uint32_t nrf52_reset_reason(void)
{
    return _sys_hal_reset_reason;
}

bool nrf52_usb_power_present(void)
{
    return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}

void nrf52_system_off(uint32_t wake_gpio)
{
    __disable_irq();

    // A low level on the wake pin is the only thing that starts the chip again
    nrf52_gpio_port(wake_gpio)->PIN_CNF[wake_gpio & 31u] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);

    NRF_POWER->SYSTEMOFF = 1;
    __DSB();
    // Reached only with a debugger attached (emulated System OFF)
    for (;;)
    {
        __WFE();
    }
}

bool sys_hal_init()
{
    _sys_hal_reset_reason = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = _sys_hal_reset_reason;

    // The crystal: USB needs it, and the microsecond clock keeps time by it. Carry on from the RC
    // oscillator if it never starts.
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    SYS_HAL_TIMER->TASKS_STOP = 1;
    SYS_HAL_TIMER->MODE = TIMER_MODE_MODE_Timer;
    SYS_HAL_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    SYS_HAL_TIMER->PRESCALER = SYS_HAL_TIMER_PRESCALER;
    SYS_HAL_TIMER->CC[SYS_HAL_CC_WRAP] = 0;
    SYS_HAL_TIMER->TASKS_CLEAR = 1;
    SYS_HAL_TIMER->TASKS_START = 1;
    SYS_HAL_TIMER->EVENTS_COMPARE[SYS_HAL_CC_WRAP] = 0;
    SYS_HAL_TIMER->INTENSET = TIMER_INTENSET_COMPARE1_Msk;
    NVIC_SetPriority(SYS_HAL_TIMER_IRQ, 1);
    NVIC_ClearPendingIRQ(SYS_HAL_TIMER_IRQ);
    NVIC_EnableIRQ(SYS_HAL_TIMER_IRQ);

    const uint64_t hfxo_until = sys_hal_now_us() + SYS_HAL_HFXO_START_US;
    while (!NRF_CLOCK->EVENTS_HFCLKSTARTED && (sys_hal_now_us() < hfxo_until))
    {
    }

    // A watchdog that is already running (a bootloader's) keeps its settings: they can't be changed
    if (!NRF_WDT->RUNSTATUS)
    {
        NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
                          (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);
        NRF_WDT->CRV = (SYS_HAL_WDT_TIMEOUT_S * 32768u) - 1u;
        NRF_WDT->RREN = WDT_RREN_RR0_Msk;
        NRF_WDT->TASKS_START = 1;
    }
    NRF_WDT->RR[0] = SYS_HAL_WDT_RELOAD;

    NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Enabled << RNG_CONFIG_DERCEN_Pos;

    return true;
}

// One core: only the first loop is given (hoja.c runs both loops' passes from it)
void sys_hal_start_dualcore(void (*task_core_0)(void), void (*task_core_1)(void))
{
    (void)task_core_1;
    if (task_core_0 != NULL)
        task_core_0();
}

uint32_t sys_hal_core_num(void)
{
    return 0;
}

uint32_t sys_hal_irq_save(void)
{
    const uint32_t masked = __get_PRIMASK();
    __disable_irq();
    return masked;
}

void sys_hal_irq_restore(uint32_t state)
{
    if (!state)
        __enable_irq();
}

uint32_t sys_hal_random()
{
    uint32_t value = 0;

    // The Bluetooth controller fills its own pool from this generator, by interrupt, and leaves the
    // interrupt armed while it is filling: held off here, and the generator left running for it
    const bool irq_was_on = NVIC_GetEnableIRQ(RNG_IRQn) != 0;
    NVIC_DisableIRQ(RNG_IRQn);

    NRF_RNG->TASKS_START = 1;
    for (uint8_t i = 0; i < 4; i++)
    {
        NRF_RNG->EVENTS_VALRDY = 0;
        while (!NRF_RNG->EVENTS_VALRDY)
        {
        }
        value = (value << 8) | (NRF_RNG->VALUE & 0xFFu);
    }
    if (!(NRF_RNG->INTENSET & RNG_INTENSET_VALRDY_Msk))
        NRF_RNG->TASKS_STOP = 1;
    if (irq_was_on)
        NVIC_EnableIRQ(RNG_IRQn);

    return value;
}

uint32_t sys_hal_get_bootmemory()
{
    const volatile uint32_t *memory = _sys_hal_bootmemory_at();
    if (memory[0] != ~memory[1])
        return 0;
    return memory[0];
}

void sys_hal_set_bootmemory(uint32_t in)
{
    volatile uint32_t *memory = _sys_hal_bootmemory_at();
    memory[0] = in;
    memory[1] = ~in;
}
