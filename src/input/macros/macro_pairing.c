#include "input/macros/macro_pairing.h"
#include "utilities/interval.h"
#include "utilities/boot.h"
#include "utilities/static_config.h"
#include "hoja_shared_types.h"
#include "hoja.h"

#define PAIRING_HOLD_TIME 3 // Seconds
#define PAIRING_MACRO_INTERVAL_US 3000
#define PAIRING_HOLD_LOOPS ( (PAIRING_HOLD_TIME*1000*1000) / PAIRING_MACRO_INTERVAL_US )

static bool _macro_pairing_enabled(void)
{
    const hoja_config_s *cfg = hoja_config_get();
    return cfg && cfg->sync_macro_code[0] != INPUT_CODE_UNUSED;
}

static bool _macro_code_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
    {
        return false;
    }

    switch (input_static.input_info[code].input_type)
    {
    case MAPPER_INPUT_TYPE_HOVER:
    case MAPPER_INPUT_TYPE_JOYSTICK:
        return input->inputs[code] > 0;

    default:
        return input->presses[code];
    }
}

static bool _macro_pairing_pressed(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->sync_macro_code[0] == INPUT_CODE_UNUSED)
    {
        return false;
    }

    if (!_macro_code_pressed(input, cfg->sync_macro_code[0]))
    {
        return false;
    }

    if (cfg->sync_macro_code[1] == INPUT_CODE_UNUSED)
    {
        return true;
    }

    return _macro_code_pressed(input, cfg->sync_macro_code[1]);
}

void macro_pairing(uint64_t timestamp, mapper_input_s *input)
{
    if (!_macro_pairing_enabled())
    {
        return;
    }

    static interval_s interval = {0};
    static bool holding = false;
    static uint32_t iterations = 0;
    static bool lockout = false;
    static bool boot_wait = true;

    bool interval_reset = false;
    const bool pressed = _macro_pairing_pressed(input);

    if (boot_wait)
    {
        if (!pressed)
        {
            boot_wait = false;
            interval_reset = true;
        }
        else
        {
            return;
        }
    }

    if (lockout)
    {
        return;
    }

    if (interval_resettable_run(timestamp, PAIRING_MACRO_INTERVAL_US, interval_reset, &interval))
    {
        if (!holding && pressed)
        {
            holding = true;
        }
        else if (holding && !pressed)
        {
            holding = false;
            iterations = 0;
        }

        if (holding)
        {
            iterations++;

            if (iterations >= PAIRING_HOLD_LOOPS)
            {
                core_reportformat_t format = core_current_params()->core_report_format;
                bool pair = false;

                if (format == CORE_REPORTFORMAT_SWPRO)
                {
                    pair = true;
                }

                if (pair)
                {
                    lockout = true;

                    boot_memory_s mem = {
                        .gamepad_method = GAMEPAD_METHOD_BLUETOOTH,
                        .report_format  = (uint8_t)format,
                        .gamepad_pair   = true
                    };

                    boot_set_memory(&mem);
                    hoja_deinit(hoja_restart);
                }
            }
        }
    }
}
