#include "input/macros.h"
#include "utilities/interval.h"
#include "input_shared_types.h"
#include "hoja_shared_types.h"
#include "utilities/callback.h"

typedef enum
{
    MACRO_IDLE = 0,
    MACRO_HELD = 1,
} macro_press_t;

macro_press_t _safe_mode = MACRO_IDLE;
bool _safe_mode_state = false;
macro_press_t _ship_mode = MACRO_IDLE;

macro_press_t _sync_mode = MACRO_IDLE;

callback_t macro_deinit_cb;
volatile bool macro_deinit_done = false;
void macro_deinit_complete()
{
    macro_deinit_done = true;
}

void macro_handler_task(uint32_t timestamp, button_data_s *in)
{

}

bool macro_safe_mode_check()
{
    return _safe_mode_state;
}