#ifndef WEBUSB_H
#define WEBUSB_H

#include "hoja_includes.h"

typedef enum
{
  // Set FW update mode
  WEBUSB_CMD_FW_SET = 0x0F,
  // Get firmware version
  WEBUSB_CMD_FW_GET = 0xAF,

  // Set Baseband update mode
  WEBUSB_CMD_BB_SET = 0xBB,

  // Get Device Capability
  WEBUSB_CMD_CAPABILITIES_GET = 0xBE,

  WEBUSB_CMD_RGB_SET = 0x01,
  WEBUSB_CMD_RGB_GET = 0xA1,

  // Set analog invert setting
  WEBUSB_CMD_ANALOG_INVERT_SET = 0x02,
  // Get all analog invert settings
  WEBUSB_CMD_ANALOG_INVERT_GET = 0xA2,

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

  WEBUSB_CMD_SUBANGLE_SET = 0x0C,
  WEBUSB_CMD_SUBANGLE_GET = 0xAC,

  WEBUSB_CMD_OCTOANGLE_SET = 0x0D,
  WEBUSB_CMD_OCTOANGLE_GET = 0xAD,

  WEBUSB_CMD_USERCYCLE_SET = 0x0E,
  WEBUSB_CMD_USERCYCLE_GET = 0xAE,

  WEBUSB_CMD_RGBMODE_SET = 0x10,
  WEBUSB_CMD_RGBMODE_GET = 0xB0,

  WEBUSB_CMD_DEADZONE_SET = 0x20,
  WEBUSB_CMD_DEADZONE_GET = 0x2A,

  WEBUSB_CMD_BOOTMODE_SET = 0x30,
  WEBUSB_CMD_BOOTMODE_GET = 0x3A,

  WEBUSB_CMD_TRIGGER_CALIBRATION_START  = 0x3B,
  WEBUSB_CMD_TRIGGER_CALIBRATION_STOP   = 0x3C,

  // Command for analog input report
  WEBUSB_CMD_INPUT_REPORT = 0xE0,

  WEBUSB_CMD_DEBUG_REPORT = 0xEE,

  // Get the hardware test results
  WEBUSB_CMD_HWTEST_GET = 0xF2,
  WEBUSB_CMD_RUMBLETEST_GET = 0xF3,

  // Get battery status
  WEBUSB_CMD_BATTERY_STATUS_GET = 0xF4,

  WEBUSB_CMD_SAVEALL = 0xF1,
} webusb_cmd_t;

void webusb_save_confirm();
void webusb_command_processor(uint8_t *data);
void webusb_input_report_task(uint32_t timestamp, a_data_s *analog, button_data_s *buttons);
bool webusb_output_enabled();

bool webusb_ready();
bool webusb_ready_blocking();
void webusb_enable_output(bool enable);

void webusb_send_debug_dump(uint8_t len, uint8_t *data);

#endif