#include "input/macros/macro_shutdown.h"
#include "devices/battery.h"
#include "hoja.h"

#define SHUTDOWN_HOLD_TIME 3 // Seconds
#define SHUTDOWN_HOLD_US   ((uint64_t)SHUTDOWN_HOLD_TIME * 1000000ULL)

bool _shutdown_ready = false;

void _shutdown_finalize()
{
    _shutdown_ready = true;
}

static bool _macro_shipping_enabled(void)
{
    const hoja_config_s *cfg = hoja_config_get();
    return cfg && cfg->shipping_macro_code[0] != INPUT_CODE_UNUSED;
}

static bool _macro_code_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
    {
        return false;
    }

    return input->presses[code] || (input->inputs[code] > 0);
}

static bool _macro_shipping_pressed(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->shipping_macro_code[0] == INPUT_CODE_UNUSED)
    {
        return false;
    }

    if (!_macro_code_pressed(input, cfg->shipping_macro_code[0]))
    {
        return false;
    }

    if (cfg->shipping_macro_code[1] == INPUT_CODE_UNUSED)
    {
        return true;
    }

    return _macro_code_pressed(input, cfg->shipping_macro_code[1]);
}

void macro_shutdown(uint64_t timestamp, mapper_input_s *input)
{
    if (!_macro_shipping_enabled())
    {
        return;
    }

    static bool boot_wait = true;
    static bool lockout = false;
    static uint64_t hold_start_us = 0;

    const bool pressed = _macro_shipping_pressed(input);

    // Wait for the shipping combo to be released after boot.
    if (boot_wait)
    {
        if (!pressed)
        {
            boot_wait = false;
        }
        return;
    }

    if (lockout)
    {
        // Only shut down after ship-mode deinit completes and the combo is released.
        if (_shutdown_ready && !pressed)
        {
            _shutdown_ready = false;
            hoja_shutdown();
        }
        return;
    }

    if (pressed)
    {
        if (hold_start_us == 0)
        {
            hold_start_us = timestamp;
        }
        else if ((timestamp - hold_start_us) >= SHUTDOWN_HOLD_US)
        {
            hold_start_us = 0;
            lockout = true;
            hoja_deinit(_shutdown_finalize);
        }
    }
    else
    {
        hold_start_us = 0;
    }
}
