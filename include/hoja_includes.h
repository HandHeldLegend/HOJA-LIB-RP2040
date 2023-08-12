#ifndef HOJA_INCLUDES_H
#define HOJA_INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/rand.h"

#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

#include "hoja_types.h"
#include "hoja_settings.h"
#include "hoja_usb.h"
#include "hoja_defines.h"
#include "hoja_comms.h"

#include "board_config.h"

#include "stick_scaling.h"
#include "analog.h"
#include "snapback.h"
#include "imu.h"
#include "safemode.h"

#include "ws2812.pio.h"
#include "nserial.pio.h"
#include "joybus.pio.h"
#include "rgb.h"

#include "bsp/board.h"
#include "tusb.h"
#include "desc_bos.h"
// XInput TinyUSB Driver
#include "xinput_device.h"

#include "gcinput.h"
#include "xinput.h"
#include "dinput.h"
#include "swpro.h"

#include "webusb.h"

#include "gamecube.h"
#include "n64.h"

#include "remap.h"
#include "hoja.h"

// Switch pro includes
#include "switch_analog.h"
#include "switch_spi.h"
#include "switch_commands.h"



#endif
