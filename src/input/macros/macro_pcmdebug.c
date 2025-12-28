#include "input/macros/macro_pcmdebug.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"
#include "board_config.h"

#if defined(HOJA_HAPTICS_DEBUG_STEP)
float _inc_val_pcm = HOJA_HAPTICS_DEBUG_STEP;
#else 
float _inc_val_pcm = 0.0025f;
#endif

void macro_pcmdebug(uint64_t timestamp, mapper_input_s *input)
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
        // Only run if select is pressed
        if(!MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_SELECT)) return;


        if(MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_LEFT) && !dl)
        {
            dl = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_LO, -_inc_val_pcm);
        }
        else if (!MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_LEFT) && dl)
        {
            dl = false;
        }

        if(MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_RIGHT) && !dr)
        {
            dr = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_LO,_inc_val_pcm);
        }
        else if (!MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_RIGHT) && dr)
        {
            dr = false;
        }

        if(MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_DOWN) && !tl)
        {
            tl = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_HI, -_inc_val_pcm);
        }
        else if (!MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_DOWN) && tl)
        {
            tl = false;
        }

        if(MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_UP) && !tr)
        {
            tr = true;
            pcm_debug_adjust_param(PCM_DEBUG_PARAM_MIN_HI, _inc_val_pcm);
        }
        else if (!MAPPER_BUTTON_DOWN(input->inputs, INPUT_CODE_UP) && tr)
        {
            tr = false;
        }

    }
}