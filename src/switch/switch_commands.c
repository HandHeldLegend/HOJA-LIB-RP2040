#include "switch/switch_commands.h"
#include "switch/switch_spi.h"
#include "switch/switch_haptics.h"
#include "switch/switch_motion.h"

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

// DEPRECIATED
void clear_report()
{
  memset(_switch_command_buffer, 0, sizeof(_switch_command_buffer));
}

void _swcmd_clear_report(uint8_t *target, uint16_t len)
{
  memset(target, 0, len);
}

// DEPRECIATED
void set_report_id(uint8_t report_id)
{
  _switch_command_report_id = report_id;
}

void _swcmd_set_report_id(uint8_t report_id, uint8_t *target)
{
  target[0] = report_id;
}

// DEPRECIATED
void set_ack(uint8_t ack)
{
  _switch_command_buffer[12] = ack;
}

void _swcmd_set_ack(uint8_t ack, uint8_t *target)
{
  target[13] = ack;
}

// DEPRECIATED
void set_command(uint8_t command)
{
  _switch_command_buffer[13] = command;
}

void _swcmd_set_command(uint8_t command, uint8_t *target)
{
  target[14] = command;
}

// DEPRECIATED
void set_timer()
{
  static uint64_t this_ms = 0;

  sys_hal_time_ms(&this_ms);

  static uint64_t _switch_timer = 0;
  _switch_command_buffer[0] = (uint8_t)_switch_timer;

  _switch_timer += this_ms;
  if (_switch_timer > 0xFF)
  {
    _switch_timer %= 0xFF; // or -= 0xFF depending on requirements
  }
}

void _swcmd_set_timer(uint8_t *target)
{
  static uint64_t this_ms = 0;

  sys_hal_time_ms(&this_ms);

  static uint64_t _switch_timer = 0;
  target[1] = (uint8_t)_switch_timer;

  _switch_timer += this_ms;
  if (_switch_timer > 0xFF)
  {
    _switch_timer %= 0xFF; // or -= 0xFF depending on requirements
  }
}

// DEPRECIATED
void set_battconn()
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
  _switch_command_buffer[1] = s.val;
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
  target[2] = s.val;
}

void set_devinfo()
{
  // New firmware causes issue with gyro needs more research
  _switch_command_buffer[14] = 0x04; // NS Firmware primary   (4.x)
  _switch_command_buffer[15] = 0x33; // NS Firmware secondary (x.21)

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
  _switch_command_buffer[16] = 0x03; // Controller ID primary (Pro Controller)
  _switch_command_buffer[17] = 0x02; // Controller ID secondary

  /*_switch_command_buffer[18-23] = MAC ADDRESS;*/
  _switch_command_buffer[18] = gamepad_config->switch_mac_address[0];
  _switch_command_buffer[19] = gamepad_config->switch_mac_address[1];
  _switch_command_buffer[20] = gamepad_config->switch_mac_address[2];
  _switch_command_buffer[21] = gamepad_config->switch_mac_address[3];
  _switch_command_buffer[22] = gamepad_config->switch_mac_address[4];
  _switch_command_buffer[23] = gamepad_config->switch_mac_address[5] + 255; // Add 255 to essentially subtract 1 from the last byte

  _switch_command_buffer[24] = 0x00;
  _switch_command_buffer[25] = 0x02; // It's 2 now? Ok.
}

void _swcmd_set_devinfo(uint8_t *target)
{
  // New firmware causes issue with gyro needs more research
  target[15] = 0x04; // NS Firmware primary   (4.x)
  target[16] = 0x33; // NS Firmware secondary (x.21)

  //_switch_command_buffer[15] = 0x03; // NS Firmware primary   (3.x)
  //_switch_command_buffer[16] = 72;   // NS Firmware secondary (x.72)

  // Joycon R - 0x01, 0x02
  // Joycon L - 0x02, 0x02
  // Procon   - 0x03, 0x02
  // N64      - 0x0C, 0x11
  // SNES     - 0x0B, 0x02
  // Famicom  - 0x07, 0x02
  // NES      - 0x09, 0x02
  // Genesis  - 0x0D, 0x02
  _switch_command_buffer[17] = 0x03; // Controller ID primary (Pro Controller)
  _switch_command_buffer[18] = 0x02; // Controller ID secondary

  /*_switch_command_buffer[19-24] = MAC ADDRESS;*/
  _switch_command_buffer[19] = gamepad_config->switch_mac_address[0];
  _switch_command_buffer[20] = gamepad_config->switch_mac_address[1];
  _switch_command_buffer[21] = gamepad_config->switch_mac_address[2];
  _switch_command_buffer[22] = gamepad_config->switch_mac_address[3];
  _switch_command_buffer[23] = gamepad_config->switch_mac_address[4];
  _switch_command_buffer[24] = gamepad_config->switch_mac_address[5] + 255; // Add 255 to essentially subtract 1 from the last byte

  _switch_command_buffer[25] = 0x00;
  _switch_command_buffer[26] = 0x02; // It's 2 now? Ok.
}

// DEPRECIATED
void set_sub_triggertime(uint16_t time_10_ms)
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
    _switch_command_buffer[14 + i] = upper_ms;
    _switch_command_buffer[15 + i] = lower_ms;
  }
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
    target[15 + i] = upper_ms;
    target[16 + i] = lower_ms;
  }
}

void set_shipmode(uint8_t ship_mode)
{
  // Unhandled.
}

// Sends mac address with 0x81 command (unknown?)
void info_set_mac()
{
  _switch_command_buffer[0] = 0x01;
  _switch_command_buffer[1] = 0x00;
  _switch_command_buffer[2] = 0x03;

  // Mac in LE
  _switch_command_buffer[3] = gamepad_config->switch_mac_address[5] + 255; // Add 255 to essentially subtract 1 from the last byte
  _switch_command_buffer[4] = gamepad_config->switch_mac_address[4];
  _switch_command_buffer[5] = gamepad_config->switch_mac_address[3];
  _switch_command_buffer[6] = gamepad_config->switch_mac_address[2];
  _switch_command_buffer[7] = gamepad_config->switch_mac_address[1];
  _switch_command_buffer[8] = gamepad_config->switch_mac_address[0];
}

void _swcmd_info_set_mac(uint8_t *target)
{
  target[1] = 0x01;
  target[2] = 0x00;
  target[3] = 0x03;

  // Mac in LE
  target[4] = gamepad_config->switch_mac_address[5] + 255; // Add 255 to essentially subtract 1 from the last byte
  target[5] = gamepad_config->switch_mac_address[4];
  target[6] = gamepad_config->switch_mac_address[3];
  target[7] = gamepad_config->switch_mac_address[2];
  target[8] = gamepad_config->switch_mac_address[1];
  target[9] = gamepad_config->switch_mac_address[0];
}

// A second part to the initialization,
// no idea what this does but it's needed
// to get continued comms over USB.
// DEPRECIATED
void info_set_init()
{
  _switch_command_buffer[0] = 0x02;
}

void _swcmd_info_set_init(uint8_t *target)
{
  target[1] = 0x02;
}

void info_handler(uint8_t info_code, hid_report_tunnel_cb cb)
{
  clear_report();
  set_report_id(0x81);

  switch (info_code)
  {
  case 0x01:
    // printf("MAC Address requested.");
    info_set_mac();
    break;

  default:
    // printf("Unknown setup requested: %X", info_code);
    _switch_command_buffer[0] = info_code;
    break;
  }

  switch (hoja_get_status().gamepad_method)
  {
  default:
  case GAMEPAD_METHOD_USB:
    cb(_switch_command_report_id, _switch_command_buffer, SWPRO_REPORT_SIZE_USB);
    break;

  case GAMEPAD_METHOD_BLUETOOTH:
    cb(_switch_command_report_id, _switch_command_buffer, SWPRO_REPORT_SIZE_BT);
    break;
  }
}

void _swcmd_info_handler(uint8_t info_code, uint8_t *target)
{
  _swcmd_clear_report(target, 64);
  _swcmd_set_report_id(0x81, target);

  switch (info_code)
  {
  case 0x01:
    // printf("MAC Address requested.");
    _swcmd_info_set_mac(target);
    break;

  default:
    // printf("Unknown setup requested: %X", info_code);
    target[1] = info_code;
    break;
  }
}

void pairing_set(uint8_t phase, const uint8_t *host_address)
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

    set_ack(0x81);
    _switch_command_buffer[14] = 1;

    // Mac in LE
    _switch_command_buffer[15] = gamepad_config->switch_mac_address[5] + 255; // Add 255 to essentially subtract 1 from the last byte
    _switch_command_buffer[16] = gamepad_config->switch_mac_address[4];
    _switch_command_buffer[17] = gamepad_config->switch_mac_address[3];
    _switch_command_buffer[18] = gamepad_config->switch_mac_address[2];
    _switch_command_buffer[19] = gamepad_config->switch_mac_address[1];
    _switch_command_buffer[20] = gamepad_config->switch_mac_address[0];

    memcpy(&_switch_command_buffer[15 + 6], pro_controller_string, 24);
    break;
  case 2:
    set_ack(0x81);
    _switch_command_buffer[14] = 2;
    memcpy(&_switch_command_buffer[15], _switch_ltk, 16);
    break;
  case 3:
    set_ack(0x81);
    _switch_command_buffer[14] = 3;
    break;
  }
}

// Handles a command, always 0x21 as a response ID
void command_handler(uint8_t command, const uint8_t *data, uint16_t len, hid_report_tunnel_cb cb)
{
  // Clear report
  clear_report();

  // Set report ID
  set_report_id(0x21);

  // Update timer
  set_timer();

  // Set connection/power info
  set_battconn();

  // Set subcmd
  set_command(command);
  // printf("CMD: ");

  switch (command)
  {
  case SW_CMD_SET_NFC:
    // printf("Set NFC MCU:\n");
    set_ack(0x80);
    break;

  case SW_CMD_ENABLE_IMU:
    // printf("Enable IMU: %d\n", data[11]);
    // imu_set_enabled(data[11] > 0);
    _switch_imu_mode = data[11];
    set_ack(0x80);
    break;

  case SW_CMD_SET_PAIRING:
    // printf("Set pairing.\n");
    pairing_set(data[11], &data[12]);
    break;

  case SW_CMD_SET_INPUTMODE:
    // printf("Input mode change: %X\n", data[11]);
    set_ack(0x80);
    _switch_reporting_mode = data[11];
    break;

  case SW_CMD_GET_DEVICEINFO:
    // printf("Get device info.\n");
    set_ack(0x82);
    set_devinfo();
    break;

  case SW_CMD_SET_SHIPMODE:
    // printf("Set ship mode: %X\n", data[11]);
    set_ack(0x80);
    break;

  case SW_CMD_GET_SPI:
    // printf("Read SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    set_ack(0x90);
    sw_spi_readfromaddress(data[12], data[11], data[15]);
    break;

  case SW_CMD_SET_HCI:
    // For now all options should shut down
    hoja_deinit(hoja_shutdown);
    break;

  case SW_CMD_SET_SPI:
    // printf("Write SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    set_ack(0x80);

    // Write IMU calibration data
    // if ((data[12] == 0x80) && (data[11] == 0x26))
    //{
    //  for(uint16_t i = 0; i < 26; i++)
    //  {
    //    global_loaded_settings.imu_calibration[i] = data[16+i];
    //    //printf("0x%x, ", data[16+i]);
    //    //printf("\n");
    //  }
    //  settings_save(false);
    //}

    break;

  case SW_CMD_GET_TRIGGERET:
    // printf("Get trigger ET.\n");
    set_ack(0x83);
    set_sub_triggertime(100);
    break;

  case SW_CMD_ENABLE_VIBRATE:
    // printf("Enable vibration.\n");
    set_ack(0x80);
    break;

  case SW_CMD_SET_PLAYER:
    // printf("Set player: ");
    set_ack(0x80);

    uint8_t player = data[11] & 0xF;
    uint8_t set_num = 0;

    switch (player)
    {
    default:
      set_num = 1; // Always set *something*
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

    if (set_num > 0)
    {
      hoja_set_connected_status(set_num);
    }

    break;

  default:
    // printf("Unhandled: %X\n", command);
    for (uint16_t i = 0; i < len; i++)
    {
      // printf("%X, ", data[i]);
    }
    // printf("\n");
    set_ack(0x80);
    break;
  }

  // printf("Sent: ");
  // for (uint8_t i = 0; i < 32; i++)
  {
    // printf("%X, ", _switch_command_buffer[i]);
  }
  // printf("\n");

  switch (hoja_get_status().gamepad_method)
  {
  default:
  case GAMEPAD_METHOD_USB:
    cb(0x21, _switch_command_buffer, SWPRO_REPORT_SIZE_USB);
    break;

  case GAMEPAD_METHOD_BLUETOOTH:
    cb(0x21, _switch_command_buffer, SWPRO_REPORT_SIZE_BT);
    break;
  }
}

void _swcmd_report_handler(uint8_t report_id, const uint8_t *in_data, uint16_t in_len, uint8_t *out_data)
{
  switch (report_id)
  {
  // We have command data and possibly rumble
  case SW_OUT_ID_RUMBLE_CMD:
    switch_haptics_rumble_translate(&in_data[2]);
    //command_handler(data[10], data, len, cb);
    break;

  case SW_OUT_ID_RUMBLE:
    // Handled elsewhere for immediate servicing
    break;

  case SW_OUT_ID_INFO:
    //info_handler(in_data[1], cb);
    break;

  default:
    // printf("Unknown report: %X\n", report_id);
    break;
  }
}

// Handles an OUT report and responds accordingly.
// WILL BE MIGRATED TO _switch_commands_report_handler
void report_handler(uint8_t report_id, const uint8_t *data, uint16_t len, hid_report_tunnel_cb cb)
{
  switch (report_id)
  {
  // We have command data and possibly rumble
  case SW_OUT_ID_RUMBLE_CMD:
    switch_haptics_rumble_translate(&data[2]);
    command_handler(data[10], data, len, cb);
    break;

  case SW_OUT_ID_RUMBLE:
    // switch_haptics_rumble_translate(&data[2]);
    break;

  case SW_OUT_ID_INFO:
    info_handler(data[1], cb);
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

#define DEBUG_IMU_VAL 8

// PUBLIC FUNCTIONS
void switch_commands_process(uint64_t timestamp, sw_input_s *input_data, hid_report_tunnel_cb cb)
{
  if (_switch_in_command_got)
  {
    report_handler(_switch_in_report_id, _switch_in_command_buffer, _switch_in_command_len, cb);
    _switch_in_command_got = false;
  }
  else
  {
    if (_switch_reporting_mode == 0x30)
    {
      clear_report();
      set_report_id(0x30);
      set_timer();
      set_battconn();

      if (_switch_imu_mode == 0x01)
      {
        imu_data_s imu = {0};

        imu_access_safe(&imu);

        // Group 1
        _switch_command_buffer[12] = imu.ay_8l; // Y-axis
        _switch_command_buffer[13] = imu.ay_8h;
        _switch_command_buffer[14] = imu.ax_8l; // X-axis
        _switch_command_buffer[15] = imu.ax_8h;
        _switch_command_buffer[16] = imu.az_8l; // Z-axis
        _switch_command_buffer[17] = imu.az_8h;
        _switch_command_buffer[18] = imu.gy_8l;
        _switch_command_buffer[19] = imu.gy_8h;
        _switch_command_buffer[20] = imu.gx_8l;
        _switch_command_buffer[21] = imu.gx_8h;
        _switch_command_buffer[22] = imu.gz_8l;
        _switch_command_buffer[23] = imu.gz_8h;

        // Group 2
        memcpy(&_switch_command_buffer[24], &_switch_command_buffer[12], 12);
        /*
        _switch_command_buffer[24] = imu[1].ay_8l; // Y-axis
        _switch_command_buffer[25] = imu[1].ay_8h;
        _switch_command_buffer[26] = imu[1].ax_8l; // X-axis
        _switch_command_buffer[27] = imu[1].ax_8h;
        _switch_command_buffer[28] = imu[1].az_8l; // Z-axis
        _switch_command_buffer[29] = imu[1].az_8h;
        _switch_command_buffer[30] = imu[1].gy_8l;
        _switch_command_buffer[31] = imu[1].gy_8h;
        _switch_command_buffer[32] = imu[1].gx_8l;
        _switch_command_buffer[33] = imu[1].gx_8h;
        _switch_command_buffer[34] = imu[1].gz_8l;
        _switch_command_buffer[35] = imu[1].gz_8h;
        */

        // Group 3
        memcpy(&_switch_command_buffer[36], &_switch_command_buffer[12], 12);
        /*
        _switch_command_buffer[36] = imu[2].ay_8l; // Y-axis
        _switch_command_buffer[37] = imu[2].ay_8h;
        _switch_command_buffer[38] = imu[2].ax_8l; // X-axis
        _switch_command_buffer[39] = imu[2].ax_8h;
        _switch_command_buffer[40] = imu[2].az_8l; // Z-axis
        _switch_command_buffer[41] = imu[2].az_8h;
        _switch_command_buffer[42] = imu[2].gy_8l;
        _switch_command_buffer[43] = imu[2].gy_8h;
        _switch_command_buffer[44] = imu[2].gx_8l;
        _switch_command_buffer[45] = imu[2].gx_8h;
        _switch_command_buffer[46] = imu[2].gz_8l;
        _switch_command_buffer[47] = imu[2].gz_8h;
        */
      }
      else if (_switch_imu_mode == 0x02)
      {
        // New Gyro test code
        static mode_2_s mode_2_data = {0};
        static quaternion_s quat = {0};

        imu_quaternion_access_safe(&quat);

        switch_motion_pack_quat(&quat, &mode_2_data);
        memcpy(&(_switch_command_buffer[12]), &mode_2_data, sizeof(mode_2_s));
      }

      // Set input data
      _switch_command_buffer[2] = input_data->right_buttons;
      _switch_command_buffer[3] = input_data->shared_buttons;
      _switch_command_buffer[4] = input_data->left_buttons;

      // Set sticks directly from hoja_analog_data
      // Saves cycles :)
      _switch_command_buffer[5] = (input_data->ls_x & 0xFF);

      _switch_command_buffer[6] = (input_data->ls_x & 0xF00) >> 8;
      _switch_command_buffer[6] |= (input_data->ls_y & 0xF) << 4;

      _switch_command_buffer[7] = (input_data->ls_y & 0xFF0) >> 4;
      _switch_command_buffer[8] = (input_data->rs_x & 0xFF);

      _switch_command_buffer[9] = (input_data->rs_x & 0xF00) >> 8;
      _switch_command_buffer[9] |= (input_data->rs_y & 0xF) << 4;

      _switch_command_buffer[10] = (input_data->rs_y & 0xFF0) >> 4;
      //_switch_command_buffer[11] = _unknown_thing();

      // //printf("V: %d, %d\n", _switch_command_buffer[46], _switch_command_buffer[47]);
      switch (hoja_get_status().gamepad_method)
      {
      default:
      case GAMEPAD_METHOD_USB:
        cb(0x30, _switch_command_buffer, SWPRO_REPORT_SIZE_USB);
        break;

      case GAMEPAD_METHOD_BLUETOOTH:
        cb(0x30, _switch_command_buffer, SWPRO_REPORT_SIZE_BT);
        break;
      }
    }
  }
}

// PUBLIC FUNCTIONS

void switch_commands_future_handle(uint8_t report_id, const uint8_t *data, uint16_t len)
{
  _switch_in_command_got = true;
  _switch_in_report_id = report_id;
  _switch_in_command_len = len;
  memcpy(_switch_in_command_buffer, data, len);
}

void switch_commands_bulkset(uint8_t start_idx, uint8_t *data, uint8_t len)
{
  memcpy(&_switch_command_buffer[start_idx], data, len);
}
