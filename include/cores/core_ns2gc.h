#ifndef CORES_NS2GC_H
#define CORES_NS2GC_H

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"
#include "hhl_tusb.h"

// The GameCube controller of a Switch 2 (NSO) over USB, through NS2-LIB-HID.
//
// GameCube mode held at power-up (Slippi's combo) on a cable starts as this controller and finds out
// what the host is from its first requests:
//   - a computer asks for more than 8 bytes of device descriptor first (both Switches ask for 8)
//   - a Switch 1 reads a string before configuring the device; a Switch 2 reads none, and makes
//     its vendor request 0x03 right after configuring
// A Switch 2 keeps this controller. A computer gets Slippi (1 kHz), a Switch 1 the GameCube adapter
// as Nintendo's (8 ms): the USB starts again as that.
bool core_ns2gc_init(core_params_s *params);

// This mode's start on USB should be the GameCube controller (finding out the host first)
bool core_ns2gc_wanted(const core_params_s *params);

// The GameCube controller is running (USB takes core_ns2gc_usb_config)
bool core_ns2gc_running(void);

// Slippi on a console: Nintendo's 8 ms polling
bool core_ns2gc_console_rate(void);

// The USB personality for the driver library: descriptors, vendor requests, commands
void core_ns2gc_usb_config(hhl_tusb_config_s *tcfg);

// What the host asks while connecting
void core_ns2gc_usb_device_requested(uint16_t length);
void core_ns2gc_usb_string_requested(uint8_t index);
void core_ns2gc_usb_configured(void);

// Each USB task pass: leaves a Switch 1 once it has finished setting up
void core_ns2gc_usb_task(uint64_t now_us);

#if defined(HOJA_NS2)
#include "ns2_lib_types.h"
// This controller as the console knows it (type, address, serial, colours), the same on every
// transport: a pairing made on one holds on the others
void core_ns2gc_identity(ns2_device_config_s *cfg, const uint8_t mac[6]);
#endif

#endif
