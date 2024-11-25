#ifndef HOJA_TYPES_H
#define HOJA_TYPES_H

#include <inttypes.h>
#include "devices/devices.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOJA_RUMBLE_TYPE_ERM 0
#define HOJA_RUMBLE_TYPE_HAPTIC 1
#define ANALOG_DIGITAL_THRESH 700

typedef struct
{
    device_method_t  input_method;
    device_mode_t    input_mode;
} hoja_config_t;

#ifdef __cplusplus
}
#endif

#endif
