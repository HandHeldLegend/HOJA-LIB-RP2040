#include "switch/switch_commands.h"
#include "switch/switch_spi.h"
#include "switch/switch_haptics.h"
#include "switch/switch_motion.h"

#include "cores/core_switch.h"

#include "utilities/settings.h"

#include "devices/haptics.h"
#include "devices/rgb.h"
#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "hal/sys_hal.h"

#include "hoja.h"
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// This C file handles various Switch gamepad commands (OUT reports)
uint8_t _switch_in_command_buffer[64] = {0};
uint8_t _switch_in_report_id = 0x00;
uint16_t _switch_in_command_len = 64;
bool _switch_in_command_got = false;

uint8_t _switch_reporting_mode = 0x30;
uint8_t _switch_imu_mode = 0x00;

uint8_t _switch_command_buffer[64] = {0};
uint8_t _switch_command_report_id = 0x00;

uint8_t _switch_ltk[16] = {0};

void _swcmd_command_handler(uint8_t command, const uint8_t *data, uint8_t *out);

void generate_ltk()
{
  // printf("Generated LTK: ");
  for (uint8_t i = 0; i < 16; i++)
  {
    _switch_ltk[i] = sys_hal_random() & 0xFF;
    // printf("%X : ", _switch_ltk[i]);
  }
  // printf("\n");
}

void _swcmd_set_ack(uint8_t ack, uint8_t *target)
{
  target[12] = ack;
}

void _swcmd_set_command(uint8_t command, uint8_t *target)
{
  target[13] = command;
}

void _swcmd_set_timer(uint8_t *target)
{
  static uint64_t this_ms = 0;

  sys_hal_time_ms(&this_ms);

  static uint64_t _switch_timer = 0;

  _switch_timer += this_ms;
  if (_switch_timer > 0xFF)
  {
    _switch_timer %= 0xFF; // or -= 0xFF depending on requirements
  }

  target[0] = (uint8_t)_switch_timer;
}

void _swcmd_set_battconn(uint8_t *target)
{
  fuelgauge_status_s fstat = {0};
  fuelgauge_get_status(&fstat);

  battery_status_s bstat = {0};
  battery_get_status(&bstat);

  bat_status_u s = {
      .bat_lvl = 4,
      .charging = 1,
      .connection = 0};

  if (bstat.connected)
  {
    s.charging = 0;

    if (bstat.charging)
    {
      s.charging = 1;
      s.connection = 1;
    }
    else if (bstat.plugged)
    {
      s.charging = 0;
      s.connection = 1;
    }

    if (fstat.connected)
    {
      switch (fstat.simple)
      {
      default:
        s.bat_lvl = 1;
        break;

      case 1:
        s.bat_lvl = 1;
        break;

      case 2:
        s.bat_lvl = 2;
        break;

      case 3:
        s.bat_lvl = 3;
        break;

      case 4:
        s.bat_lvl = 4;
        break;
      }
    }
  }
  else
  {
    s.charging = 0;
    s.connection = 1;
  }

  // Always set to USB connected
  target[1] = s.val;
}

void _swcmd_set_devinfo(uint8_t *target)
{
  // New firmware causes issue with gyro needs more research
  target[14] = 0x04; // NS Firmware primary   (4.x)
  target[15] = 0x33; // NS Firmware secondary (x.21)

  //_switch_command_buffer[14] = 0x03; // NS Firmware primary   (3.x)
  //_switch_command_buffer[15] = 72;   // NS Firmware secondary (x.72)

  // Joycon R - 0x01, 0x02
  // Joycon L - 0x02, 0x02
  // Procon   - 0x03, 0x02
  // N64      - 0x0C, 0x11
  // SNES     - 0x0B, 0x02
  // Famicom  - 0x07, 0x02
  // NES      - 0x09, 0x02
  // Genesis  - 0x0D, 0x02
  target[16] = 0x03; // Controller ID primary (Pro Controller)
  target[17] = 0x02; // Controller ID secondary

  core_params_s *params = core_current_params();

  /*_switch_command_buffer[18-23] = MAC ADDRESS;*/
  memcpy(&target[18], params->transport_dev_mac, 6);

  target[24] = 0x00;
  target[25] = 0x02; // It's 2 now? Ok.
}

void _swcmd_set_subtriggertime(uint16_t time_10_ms, uint8_t *target)
{
  uint8_t upper_ms = 0xFF & time_10_ms;
  uint8_t lower_ms = (0xFF00 & time_10_ms) >> 8;

  // Set all button groups
  // L - 15, 16
  // R - 17, 18
  // ZL - 19, 20
  // ZR - 21, 22
  // SL - 23, 24
  // SR - 25, 26
  // Home - 27, 28

  for (uint8_t i = 0; i < 14; i += 2)
  {
    target[14 + i] = upper_ms;
    target[15 + i] = lower_ms;
  }
}

void set_shipmode(uint8_t ship_mode)
{
  // Unhandled.
}

void _swcmd_info_set_mac(uint8_t *target)
{
  target[0] = 0x01;
  target[1] = 0x00;
  target[2] = 0x03;

  core_params_s *params = core_current_params();

  // Mac in LE
  target[3] = params->transport_dev_mac[5];
  target[4] = params->transport_dev_mac[4];
  target[5] = params->transport_dev_mac[3];
  target[6] = params->transport_dev_mac[2];
  target[7] = params->transport_dev_mac[1];
  target[8] = params->transport_dev_mac[0];
}

// A second part to the initialization,
// no idea what this does but it's needed
// to get continued comms over USB.
void _swcmd_info_set_init(uint8_t *target)
{
  target[0] = 0x02;
}

void _swcmd_info_handler(uint8_t info_code, uint8_t *target)
{
  switch (info_code)
  {
  case 0x01:
    // printf("MAC Address requested.");
    _swcmd_info_set_mac(target);
    break;

  default:
    // printf("Unknown setup requested: %X", info_code);
    target[0] = info_code;
    break;
  }
}

void _swcmd_pairing_set(uint8_t phase, const uint8_t *host_address, uint8_t *target)
{
  // Respond with MAC address and "Pro Controller".
  const uint8_t pro_controller_string[24] = {0x00, 0x25, 0x08, 0x50, 0x72, 0x6F, 0x20, 0x43, 0x6F,
                                             0x6E, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x65, 0x72, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x68};

  bool diff_host = false;

  switch (phase)
  {
  default:
  case 1:

    // Get host address and compare it.
    // for (uint i = 0; i < 6; i++)
    //{
    //  if (global_loaded_settings.switch_host_address[i] != host_address[5 - i])
    //  {
    //    global_loaded_settings.switch_host_address[i] = host_address[5 - i];
    //    diff_host = true;
    //  }
    //}

    // Save if we have an updated host address.
    // if (diff_host)
    // settings_save_from_core0();

    _swcmd_set_ack(0x81, target);
    target[14] = 1;

    core_params_s *params = core_current_params();

    // Mac in LE
    target[15] = params->transport_dev_mac[5];
    target[16] = params->transport_dev_mac[4];
    target[17] = params->transport_dev_mac[3];
    target[18] = params->transport_dev_mac[2];
    target[19] = params->transport_dev_mac[1];
    target[20] = params->transport_dev_mac[0];

    memcpy(&target[15 + 6], pro_controller_string, 24);
    break;
  case 2:
    _swcmd_set_ack(0x81, target);
    target[14] = 2;
    memcpy(&target[15], _switch_ltk, 16);
    break;
  case 3:
    _swcmd_set_ack(0x81, target);
    target[14] = 3;
    break;
  }
}

void _swcmd_report_handler(uint8_t report_id, const uint8_t *in_data, uint16_t in_len, uint8_t *out_data)
{
  switch (report_id)
  {
  // We have command data
  case SW_OUT_ID_RUMBLE_CMD:
    _swcmd_command_handler(in_data[10], in_data, out_data);
    break;

  case SW_OUT_ID_RUMBLE:
    // Handled elsewhere for immediate servicing
    break;

  case SW_OUT_ID_INFO:
    _swcmd_info_handler(in_data[1], out_data);
    break;

  default:
    // printf("Unknown report: %X\n", report_id);
    break;
  }
}

uint8_t _unknown_thing()
{
  static uint8_t out = 0xA;
  if (out == 0xA)
    out = 0xB;
  else if (out == 0xB)
    out = 0xC;
  else
    out = 0xA;

  return out;
}

// PUBLIC FUNCTIONS
void swcmd_generate_inputreport(uint8_t *report_id, uint8_t *out)
{
  // Since some update, only report ID 30 seems to be used.
  *report_id = 0x30;
  _swcmd_set_timer(out);
  _swcmd_set_battconn(out);

  switch (_switch_imu_mode)
  {
  // IMU Mode 1 is raw sensor data
  case 0x01:
  {
    imu_data_s imu = {0};

    imu_access_safe(&imu);

    // Group 1
    out[12] = imu.ay_8l; // Y-axis
    out[13] = imu.ay_8h;
    out[14] = imu.ax_8l; // X-axis
    out[15] = imu.ax_8h;
    out[16] = imu.az_8l; // Z-axis
    out[17] = imu.az_8h;
    out[18] = imu.gy_8l;
    out[19] = imu.gy_8h;
    out[20] = imu.gx_8l;
    out[21] = imu.gx_8h;
    out[22] = imu.gz_8l;
    out[23] = imu.gz_8h;
    
    // Group 2
    memcpy(&out[24], &out[12], 12);
    // Group 3
    memcpy(&out[36], &out[12], 12);
  }
  break;

  // IMU Mode 2 uses a custom packed quaternion format
  case 0x02:
  {
    static mode_2_s mode_2_data = {0};
    static quaternion_s quat = {0};

    imu_quaternion_access_safe(&quat);

    switch_motion_pack_quat(&quat, &mode_2_data);
    memcpy(&(out[12]), &mode_2_data, sizeof(mode_2_s));
  }
  break;

  // Anything else is presumed to be NO gyro
  default:
    break;
  }
}

void _swcmd_command_handler(uint8_t command, const uint8_t *data, uint8_t *out)
{
  // Report ID is already set to 0x21, so
  // we are only concerned with setting the
  // actual response data
  _swcmd_set_timer(out);
  _swcmd_set_battconn(out);
  _swcmd_set_command(command, out);

  switch (command)
  {
  case SW_CMD_SET_NFC:
    // printf("Set NFC MCU:\n");
    _swcmd_set_ack(0x80, out);
    break;

  case SW_CMD_ENABLE_IMU:
    // printf("Enable IMU: %d\n", data[11]);
    // imu_set_enabled(data[11] > 0);
    _switch_imu_mode = data[11];
    _swcmd_set_ack(0x80, out);
    break;

  case SW_CMD_SET_PAIRING:
    // printf("Set pairing.\n");
    _swcmd_pairing_set(data[11], &data[12], out);
    break;

  case SW_CMD_SET_INPUTMODE:
    // printf("Input mode change: %X\n", data[11]);
    _swcmd_set_ack(0x80, out);
    _switch_reporting_mode = data[11];
    break;

  case SW_CMD_GET_DEVICEINFO:
    // printf("Get device info.\n");
    _swcmd_set_ack(0x82, out);
    _swcmd_set_devinfo(out);
    break;

  case SW_CMD_SET_SHIPMODE:
    // printf("Set ship mode: %X\n", data[11]);
    _swcmd_set_ack(0x80, out);
    break;

  case SW_CMD_GET_SPI:
    // printf("Read SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    _swcmd_set_ack(0x90, out);
    sw_spi_get(data[12], data[11], data[15], out);
    break;

  case SW_CMD_SET_HCI:
    // For now all options should shut down
    hoja_deinit(hoja_shutdown);
    break;

  case SW_CMD_SET_SPI:
    // printf("Write SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    _swcmd_set_ack(0x80, out);
    // No additional actions needed here
    // We do not save data from the switch console
    break;

  case SW_CMD_GET_TRIGGERET:
    // printf("Get trigger ET.\n");
    _swcmd_set_ack(0x83, out);
    _swcmd_set_subtriggertime(100, out);
    break;

  case SW_CMD_ENABLE_VIBRATE:
    // printf("Enable vibration.\n");
    _swcmd_set_ack(0x80, out);
    break;

  case SW_CMD_SET_PLAYER:
    // printf("Set player LED: ");
    _swcmd_set_ack(0x80, out);

    uint8_t player = data[11] & 0xF;
    uint8_t set_num = 0;

    switch (player)
    {
    default:
      // Player 0 (No LEDs)
      break;
    case 0b1:
      set_num = 1;
      break;

    case 0b11:
      set_num = 2;
      break;

    case 0b111:
      set_num = 3;
      break;

    case 0b1111:
      set_num = 4;
      break;

    case 0b1001:
      set_num = 5;
      break;
    case 0b1010:
      set_num = 6;
      break;

    case 0b1011:
      set_num = 7;
      break;
    case 0b0110:
      set_num = 8;
      break;
    }
    break;

  default:
    // printf("Unhandled: %X\n", command);
    // for (uint16_t i = 0; i < len; i++)
    //{
    //  // printf("%X, ", data[i]);
    //}
    // printf("\n");
    _swcmd_set_ack(0x80, out);
    break;
  }
}

void swcmd_generate_reply(uint8_t *in, uint8_t *report_id, uint8_t *out)
{
  switch (in[0])
  {
  case SW_OUT_ID_RUMBLE_CMD:
    // Haptics are already processed, just process command
    *report_id = 0x21;
    _swcmd_command_handler(in[10], in, out);
    break;

  case SW_OUT_ID_INFO:
    // Process info response packet
    *report_id = 0x81;
    _swcmd_info_handler(in[1], out);
    break;

  default:
    // printf("Unknown report: %X\n", report_id);
    break;
  }
}
