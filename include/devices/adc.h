#ifndef DEVICES_ADC_H
#define DEVICES_ADC_H

#include <stdbool.h>
#include <stdint.h>


bool        adc_devices_init();

uint16_t    adc_read_lx();
uint16_t    adc_read_ly();
uint16_t    adc_read_rx();
uint16_t    adc_read_ry();
uint16_t    adc_read_lt();
uint16_t    adc_read_rt();
uint16_t    adc_read_battery();

#endif