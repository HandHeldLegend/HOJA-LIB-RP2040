#ifndef CORES_NS2GC_H
#define CORES_NS2GC_H

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"
#include "hhl_tusb.h"

// The GameCube controller of a Switch 2 (NSO) over USB, through NS2-LIB-HID
bool core_ns2gc_init(core_params_s *params);

// The USB personality for the driver library: descriptors, vendor requests, commands
void core_ns2gc_usb_config(hhl_tusb_config_s *tcfg);

#endif
