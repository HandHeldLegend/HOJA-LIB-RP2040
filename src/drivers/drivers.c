#include "drivers/drivers.h"

#include "hoja_bsp.h"
#include "board_config.h"

void drivers_setup()
{
#if defined(HOJA_ADC_CHAN_LX_INIT)
    HOJA_ADC_CHAN_LX_INIT();
#endif

#if defined(HOJA_ADC_CHAN_LY_INIT)
    HOJA_ADC_CHAN_LY_INIT();
#endif

#if defined(HOJA_ADC_CHAN_RX_INIT)
    HOJA_ADC_CHAN_RX_INIT();
#endif

#if defined(HOJA_ADC_CHAN_RY_INIT)
    HOJA_ADC_CHAN_RY_INIT();
#endif

#if defined(HOJA_ADC_CHAN_LT_INIT)
    HOJA_ADC_CHAN_LT_INIT();
#endif

#if defined(HOJA_ADC_CHAN_RT_INIT)
    HOJA_ADC_CHAN_RT_INIT();
#endif

#if defined(HOJA_IMU_CHAN_A_INIT)
    HOJA_IMU_CHAN_A_INIT();
#endif

#if defined(HOJA_IMU_CHAN_B_INIT)
    HOJA_IMU_CHAN_B_INIT();
#endif

#if defined(HAPTIC_DRIVER_DRV2605L_INIT)
    HAPTIC_DRIVER_DRV2605L_INIT();
#endif
}