#ifndef WEBUSB_H
#define WEBUSB_H

#include "hoja_includes.h"

typedef enum
{
  // Set FW update mode
  WEBUSB_CMD_FW_SET = 0x0F,
  // Get firmware version
  WEBUSB_CMD_FW_GET = 0xAF,

  // Get Device Capability
  WEBUSB_CMD_CAPABILITIES_GET = 0xBE,

  WEBUSB_CMD_RGB_SET = 0x01,
  WEBUSB_CMD_RGB_GET = 0xA1,

  // Set snapback setting
  WEBUSB_CMD_SNAPBACK_SET = 0x02,
  // Get all snapback settings
  WEBUSB_CMD_SNAPBACK_GET = 0xA2,

  // Start calibration mode (standard)
  WEBUSB_CMD_CALIBRATION_START = 0x03,
  WEBUSB_CMD_CALIBRATION_STOP = 0xA3,

  // Update calibration angle (octagon)
  WEBUSB_CMD_OCTAGON_SET = 0x04,

  // Analyze waveforms
  WEBUSB_CMD_ANALYZE_START = 0x05,
  WEBUSB_CMD_ANALYZE_STOP = 0xA5,

  // Remap commands
  // Starts listening for a remap internally
  WEBUSB_CMD_REMAP_SET = 0x06,
  // Get all remap data available
  WEBUSB_CMD_REMAP_GET = 0xA6,

  // Reset all remap to default
  WEBUSB_CMD_REMAP_DEFAULT = 0x07,

  // Set gamecube Special Command function
  WEBUSB_CMD_GCSP_SET = 0x08,

  // Start IMU Calibration process
  WEBUSB_CMD_IMU_CALIBRATION_START = 0x09,

  WEBUSB_CMD_VIBRATE_SET = 0x0A,
  WEBUSB_CMD_VIBRATE_GET = 0xAA,

  WEBUSB_CMD_VIBRATEFLOOR_SET = 0x0B,
  WEBUSB_CMD_VIBRATEFLOOR_GET = 0xAB,

  // Command for analog input report
  WEBUSB_CMD_INPUT_REPORT = 0xE0,

  WEBUSB_CMD_SAVEALL = 0xF1,
} webusb_cmd_t;

void webusb_save_confirm();
void webusb_command_processor(uint8_t *data);
void webusb_input_report_task(uint32_t timestamp, a_data_s *analog);
bool webusb_output_enabled();
bool webusb_ready_blocking();
void webusb_enable_output(bool enable);

#endif