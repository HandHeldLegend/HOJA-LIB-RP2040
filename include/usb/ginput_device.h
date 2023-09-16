#ifndef GINPUT_DEVICE_H
#define GINPUT_DEVICE_H

#include "hoja_includes.h"
#include "device/usbd_pvt.h"

extern bool gc_connected;

extern const tusb_desc_device_t ginput_device_descriptor;
extern const uint8_t ginput_configuration_descriptor[];
extern const char *ginput_string_descriptor[];

extern const usbd_class_driver_t tud_ginput_driver;

void ginputd_reset(uint8_t rhport);
void ginputd_init(void);
uint32_t tud_ginput_n_write(uint8_t itf, void const* buffer, uint32_t bufsize);
uint32_t tud_ginput_n_flush (uint8_t itf);
bool tud_ginput_ready(void);

#endif