#ifndef HOJA_ANALOG_H
#define HOJA_ANALOG_H

#include <stdint.h>

#include "utilities/interval.h"
#include "input/button.h"

// Analog input data structure
typedef struct
{
    int lx;
    int ly;
    int rx;
    int ry;
} analog_data_s;

typedef enum
{
    CALIBRATE_START,
    CALIBRATE_CANCEL,
    CALIBRATE_SAVE,
} calibrate_set_t;

typedef enum
{
    ANALOG_ACCESS_RAW_DATA, // Access raw analog data
    ANALOG_ACCESS_SCALED_DATA, // Access scaled analog data
    ANALOG_ACCESS_SNAPBACK_DATA, // Access analog data post-snapback filter
} analog_access_t;

void analog_access(analog_data_s *out, analog_access_t type);

void analog_get_octoangle_data(uint8_t *axis, uint8_t *octant);
void analog_get_subangle_data(uint8_t *axis, uint8_t *octant);
void analog_send_reset();
void analog_init(analog_data_s *in, analog_data_s *out, analog_data_s *desnapped, button_data_s *buttons);
void analog_calibrate_start();
void analog_calibrate_stop();
void analog_calibrate_save();
void analog_calibrate_angle();
void analog_calibrate_center();
void analog_task(uint32_t timestamp);

#endif