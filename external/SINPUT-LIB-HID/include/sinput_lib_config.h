#ifndef SINPUT_LIB_CONFIG_H
#define SINPUT_LIB_CONFIG_H

#include "sinput_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

sinput_config_status_t sinput_config_set(const sinput_device_cfg_s *cfg);

void sinput_config_get(sinput_device_cfg_s *out);

#ifdef __cplusplus
}
#endif

#endif