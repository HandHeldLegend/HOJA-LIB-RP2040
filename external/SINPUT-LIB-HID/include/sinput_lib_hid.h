#ifndef SINPUT_LIB_HID_H
#define SINPUT_LIB_HID_H

#include <stdint.h>

#include "sinput_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const sinput_usb_device_descriptor_t *sinput_hid_get_device_descriptor(void);

void sinput_hid_get_descriptor_params(const uint8_t **hid_report_descriptor, uint16_t *hid_report_descriptor_len,
                                  const uint8_t **configuration_descriptor, uint16_t *configuration_descriptor_len,
                                  uint16_t *vid, uint16_t *pid);

#ifdef __cplusplus
}
#endif

#endif