#include "input/input.h"

#include "hal/sys_hal.h"
#include "hal/mutex_hal.h"

#include "input/analog.h"
#include "input/button.h"
#include "input/imu.h"

#include "utilities/interval.h"

#include "hoja.h"

bool input_init(gamepad_mode_t mode)
{
    button_init();
    analog_init();
}

void input_task_core0(uint32_t timestamp)
{
    button_task(timestamp);
}

void input_task_core1(uint32_t timestamp)
{
    analog_task(timestamp);
    imu_task(timestamp);
}
