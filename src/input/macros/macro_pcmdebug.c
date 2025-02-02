#include "input/macros/macro_pcmdebug.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"
#include "board_config.h"

#if defined(HOJA_HAPTICS_DEBUG_STEP)
float _inc_val_pcm = HOJA_HAPTICS_DEBUG_STEP;
#else 
float _inc_val_pcm = 0.0025f;
#endif

void macro_pcmdebug(uint32_t timestamp, button_data_s *buttons)
{
    static interval_s interval = {0};

    static bool du = false;
    static bool dd = false;
    static bool dl = false;
    static bool dr = false;
    static bool tl = false;
    static bool tr = false;
    static bool zl = false;
    static bool zr = false;

    // Dpad U/D = Maximum Lo Frequency

    // Dpad L/R = Minimum Lo Frequency
    // Trigger L/R = Minimum Hi Frequency
    


    if(interval_run(timestamp, 8000, &interval))
    {
        // Only run if minus is pressed
        if(!buttons->button_minus) return;

        if(buttons->dpad_up && !du)
        {
            du = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MAX, _inc_val_pcm);
        }
        else if (!buttons->dpad_up && du)
        {
            du = false;
        }

        if(buttons->dpad_down && !dd)
        {
            dd = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MAX, -_inc_val_pcm);
        }
        else if (!buttons->dpad_down && dd)
        {
            dd = false;
        }

        if(buttons->dpad_left && !dl)
        {
            dl = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_LO, -_inc_val_pcm);
        }
        else if (!buttons->dpad_left && dl)
        {
            dl = false;
        }

        if(buttons->dpad_right && !dr)
        {
            dr = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_LO,_inc_val_pcm);
        }
        else if (!buttons->dpad_right && dr)
        {
            dr = false;
        }

        if(buttons->trigger_l && !tl)
        {
            tl = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_HI, -_inc_val_pcm);
        }
        else if (!buttons->trigger_l && tl)
        {
            tl = false;
        }

        if(buttons->trigger_r && !tr)
        {
            tr = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_HI, _inc_val_pcm);
        }
        else if (!buttons->trigger_r && tr)
        {
            tr = false;
        }

    }
}