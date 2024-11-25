#ifndef HOJA_CAPABILITIES_H
#define HOJA_CAPABILITIES_H

#include <stdint.h>

/**
 * 
 * uint8_t array[0]:  | analog_stick_left | analog_stick_right | analog_trigger_left | analog_trigger_right | gyroscope | bluetooth | rgb | rumble |
                   |        1 bit      |        1 bit       |         1 bit       |          1 bit       |   1 bit   |   1 bit   | 1 bit | 1 bit |

 * uint8_t array[1]:  | nintendo_serial | nintendo_joybus | padding  | padding  | padding  | padding  | padding  | padding  |
                   |      1 bit         |      1 bit      |   1 bit  |   1 bit  |   1 bit  |   1 bit  |   1 bit  |   1 bit  |
*/
typedef struct
{
    uint8_t analog_stick_left      : 1;
    uint8_t analog_stick_right     : 1;
    uint8_t analog_trigger_left    : 1;
    uint8_t analog_trigger_right   : 1;
    uint8_t gyroscope : 1;
    uint8_t bluetooth : 1;
    uint8_t rgb : 1;
    uint8_t rumble_erm : 1;
    uint8_t nintendo_serial : 1;
    uint8_t nintendo_joybus : 1;
    uint8_t battery_pmic    : 1;
    uint8_t rumble_lra      : 1;
    uint8_t padding         : 4;
} hoja_capabilities_t;

#endif