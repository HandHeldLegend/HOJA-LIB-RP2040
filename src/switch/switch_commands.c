#include "switch_commands.h"

// This C file handles various Switch gamepad commands (OUT reports)
uint8_t _switch_in_command_buffer[64] = {0};
uint8_t _switch_in_report_id = 0x00;
uint16_t _switch_in_command_len = 64;
bool _switch_in_command_got = false;

uint8_t _switch_reporting_mode = 0x3F;

uint8_t _switch_command_buffer[64] = {0};
uint8_t _switch_command_report_id = 0x00;
// uint8_t _switch_mac_address[6] = {0};
uint8_t _switch_ltk[16] = {0};

void generate_ltk()
{
  printf("Generated LTK: ");
  for (uint8_t i = 0; i < 16; i++)
  {
    _switch_ltk[i] = get_rand_32() & 0xFF;
    printf("%X : ", _switch_ltk[i]);
  }
  printf("\n");
}

void clear_report()
{
  memset(_switch_command_buffer, 0, sizeof(_switch_command_buffer));
}

void set_report_id(uint8_t report_id)
{
  _switch_command_report_id = report_id;
}

void set_ack(uint8_t ack)
{
  _switch_command_buffer[12] = ack;
}

void set_command(uint8_t command)
{
  _switch_command_buffer[13] = command;
}

void set_timer()
{
  static int16_t _switch_timer = 0;
  _switch_command_buffer[0] = (uint8_t)_switch_timer;
  // printf("Td=%d \n", _switch_timer);
  _switch_timer += 2;
  if (_switch_timer > 0xFF)
  {
    _switch_timer -= 0xFF;
  }
}

void set_battconn()
{
  typedef union
  {
    struct
    {
      uint8_t connection : 4;
      uint8_t bat_lvl : 4;
    };
    uint8_t bat_status;
  } bat_status_u;

  bat_status_u s = {
      .bat_lvl = 8,
      .connection = 1};
  // Always set to USB connected
  _switch_command_buffer[1] = s.bat_status;
}

void set_devinfo()
{
  /* New firmware causes issue with gyro needs more research
  _switch_command_buffer[14] = 0x04; // NS Firmware primary   (4.x)
  _switch_command_buffer[15] = 0x33; // NS Firmware secondary (x.21) */

  _switch_command_buffer[14] = 0x03; // NS Firmware primary   (3.x)
  _switch_command_buffer[15] = 72;   // NS Firmware secondary (x.72)

  // Procon   - 0x03, 0x02
  // N64      - 0x0C, 0x11
  // SNES     - 0x0B, 0x02
  // Famicom  - 0x07, 0x02
  // NES      - 0x09, 0x02
  // Genesis  - 0x0D, 0x02
  _switch_command_buffer[16] = 0x03; // Controller ID primary (Pro Controller)
  _switch_command_buffer[17] = 0x02; // Controller ID secondary

  /*_switch_command_buffer[18-23] = MAC ADDRESS;*/

  _switch_command_buffer[24] = 0x01;
  _switch_command_buffer[25] = 0x02; // It's 2 now? Ok.
}

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

void set_shipmode(uint8_t ship_mode)
{
  // Unhandled.
}

bool shouldControllerRumble(const uint8_t *data)
{

  bool lbd = data[3] & 0x80;
  bool hbd = data[1] & 0x8;

  // Get High band amplitude
  uint8_t hba = (data[1] & 0xFE);
  uint8_t lba = (data[3] & 0x7F);

  return ((hba > 1) && !hbd) || ((lba > 0x40) && !lbd);
}

// Translate and handle rumble
void rumble_translate(const uint8_t *data)
{
  if (shouldControllerRumble(data))
  {
    uint8_t upper = (data[1] & 0xFE) / 2;
    uint8_t lower = (((data[3] & 0x7F) - 0x40) * 2) + (data[2] == 0x80);
    float il = powf((float)lower / 100.0f, 2.0f);
    float iu = powf((float)upper / 100.0f, 0.5f);

    float i = (il > iu) ? il : iu;
    i = (i >= 1.0f) ? 1.0f : i;
    cb_hoja_rumble_enable(i);
  }
  else
  {
    cb_hoja_rumble_enable(0);
  }
}

// Sends mac address with 0x81 command (unknown?)
void info_set_mac()
{
  _switch_command_buffer[0] = 0x01;
  _switch_command_buffer[1] = 0x00;
  _switch_command_buffer[2] = 0x03;

  // Mac in LE
  _switch_command_buffer[3] = global_loaded_settings.switch_mac_address[5];
  _switch_command_buffer[4] = global_loaded_settings.switch_mac_address[4];
  _switch_command_buffer[5] = global_loaded_settings.switch_mac_address[3];
  _switch_command_buffer[6] = global_loaded_settings.switch_mac_address[2];
  _switch_command_buffer[7] = global_loaded_settings.switch_mac_address[1];
  _switch_command_buffer[8] = global_loaded_settings.switch_mac_address[0];
}

// A second part to the initialization,
// no idea what this does but it's needed
// to get continued comms over USB.
void info_set_init()
{
  _switch_command_buffer[0] = 0x02;
}

void info_handler(uint8_t info_code)
{
  clear_report();
  set_report_id(0x81);

  switch (info_code)
  {
  case 0x01:
    printf("MAC Address requested.");
    info_set_mac();
    break;

  default:
    printf("Unknown setup requested: %X", info_code);
    _switch_command_buffer[0] = info_code;
    break;
  }

  tud_hid_report(_switch_command_report_id, _switch_command_buffer, 64);
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
    for (uint i = 0; i < 6; i++)
    {
      if (global_loaded_settings.switch_host_address[i] != host_address[5 - i])
      {
        global_loaded_settings.switch_host_address[i] = host_address[5 - i];
        diff_host = true;
      }
    }

    // Save if we have an updated host address.
    if (diff_host)
      settings_save();

    set_ack(0x81);
    _switch_command_buffer[14] = 1;

    // Mac in LE
    _switch_command_buffer[15] = global_loaded_settings.switch_mac_address[5];
    _switch_command_buffer[16] = global_loaded_settings.switch_mac_address[4];
    _switch_command_buffer[17] = global_loaded_settings.switch_mac_address[3];
    _switch_command_buffer[18] = global_loaded_settings.switch_mac_address[2];
    _switch_command_buffer[19] = global_loaded_settings.switch_mac_address[1];
    _switch_command_buffer[20] = global_loaded_settings.switch_mac_address[0];

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
void command_handler(uint8_t command, const uint8_t *data, uint16_t len)
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
  printf("CMD: ");

  switch (command)
  {
  case SW_CMD_SET_NFC:
    printf("Set NFC MCU:\n");
    set_ack(0x80);
    break;

  case SW_CMD_ENABLE_IMU:
    printf("Enable IMU: %d\n", data[11]);
    imu_set_enabled(data[11] > 0);
    set_ack(0x80);
    break;

  case SW_CMD_SET_PAIRING:
    printf("Set pairing.\n");
    pairing_set(data[11], &data[12]);
    break;

  case SW_CMD_SET_INPUTMODE:
    printf("Input mode change: %X\n", data[11]);
    set_ack(0x80);
    _switch_reporting_mode = data[11];
    break;

  case SW_CMD_GET_DEVICEINFO:
    printf("Get device info.\n");
    set_ack(0x82);
    set_devinfo();
    break;

  case SW_CMD_SET_SHIPMODE:
    printf("Set ship mode: %X\n", data[11]);
    set_ack(0x80);
    break;

  case SW_CMD_GET_SPI:
    printf("Read SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    set_ack(0x90);
    sw_spi_readfromaddress(data[12], data[11], data[15]);
    break;

  case SW_CMD_SET_HCI:
    // For now all options should shut down
    util_battery_enable_ship_mode();
    break;

  case SW_CMD_SET_SPI:
    printf("Write SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    set_ack(0x80);

    // Write IMU calibration data
    // if ((data[12] == 0x80) && (data[11] == 0x26))
    //{
    //  for(uint16_t i = 0; i < 26; i++)
    //  {
    //    global_loaded_settings.imu_calibration[i] = data[16+i];
    //    printf("0x%x, ", data[16+i]);
    //    printf("\n");
    //  }
    //  settings_save(false);
    //}

    break;

  case SW_CMD_GET_TRIGGERET:
    printf("Get trigger ET.\n");
    set_ack(0x83);
    set_sub_triggertime(100);
    break;

  case SW_CMD_ENABLE_VIBRATE:
    printf("Enable vibration.\n");
    set_ack(0x80);
    break;

  case SW_CMD_SET_PLAYER:
    printf("Set player: ");
    set_ack(0x80);

    uint8_t player = data[11] & 0xF;

    switch (player)
    {
    default:
    case 1:
      printf("1\n");
      break;

    case 3:
      printf("2\n");
      break;

    case 7:
      printf("3\n");
      break;

    case 15:
      printf("4\n");
      break;
    }
    break;

  default:
    printf("Unhandled: %X\n", command);
    for (uint16_t i = 0; i < len; i++)
    {
      printf("%X, ", data[i]);
    }
    printf("\n");
    set_ack(0x80);
    break;
  }

  printf("Sent: ");
  for (uint8_t i = 0; i < 32; i++)
  {
    printf("%X, ", _switch_command_buffer[i]);
  }
  printf("\n");

  tud_hid_report(0x21, _switch_command_buffer, 64);
}

// Handles an OUT report and responds accordingly.
void report_handler(uint8_t report_id, const uint8_t *data, uint16_t len)
{
  switch (report_id)
  {
  // We have command data and possibly rumble
  case SW_OUT_ID_RUMBLE_CMD:
    rumble_translate(&data[2]);
    command_handler(data[10], data, len);
    break;

  case SW_OUT_ID_RUMBLE:
    rumble_translate(&data[2]);
    break;

  case SW_OUT_ID_INFO:
    info_handler(data[1]);
    break;

  default:
    printf("Unknown report: %X\n", report_id);
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
void switch_commands_process(sw_input_s *input_data)
{
  if (_switch_in_command_got)
  {
    report_handler(_switch_in_report_id, _switch_in_command_buffer, _switch_in_command_len);
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

      // Retrieve and write IMU data
      imu_data_s *_imu_tmp = imu_fifo_last();

      // Group 1
      _switch_command_buffer[12] = _imu_tmp->ay_8l; // Y-axis
      _switch_command_buffer[13] = _imu_tmp->ay_8h;

      _switch_command_buffer[14] = _imu_tmp->ax_8l; // X-axis
      _switch_command_buffer[15] = _imu_tmp->ax_8h;

      _switch_command_buffer[16] = _imu_tmp->az_8l; // Z-axis
      _switch_command_buffer[17] = _imu_tmp->az_8h;

      _switch_command_buffer[18] = _imu_tmp->gy_8l;
      _switch_command_buffer[19] = _imu_tmp->gy_8h;

      _switch_command_buffer[20] = _imu_tmp->gx_8l;
      _switch_command_buffer[21] = _imu_tmp->gx_8h;

      _switch_command_buffer[22] = _imu_tmp->gz_8l;
      _switch_command_buffer[23] = _imu_tmp->gz_8h;

      _imu_tmp = imu_fifo_last();

      // Group 2
      _switch_command_buffer[24] = _imu_tmp->ay_8l; // Y-axis
      _switch_command_buffer[25] = _imu_tmp->ay_8h;
      _switch_command_buffer[26] = _imu_tmp->ax_8l; // X-axis
      _switch_command_buffer[27] = _imu_tmp->ax_8h;
      _switch_command_buffer[28] = _imu_tmp->az_8l; // Z-axis
      _switch_command_buffer[29] = _imu_tmp->az_8h;

      _switch_command_buffer[30] = _imu_tmp->gy_8l;
      _switch_command_buffer[31] = _imu_tmp->gy_8h;
      _switch_command_buffer[32] = _imu_tmp->gx_8l;
      _switch_command_buffer[33] = _imu_tmp->gx_8h;
      _switch_command_buffer[34] = _imu_tmp->gz_8l;
      _switch_command_buffer[35] = _imu_tmp->gz_8h;

      _imu_tmp = imu_fifo_last();

      // Group 3
      _switch_command_buffer[36] = _imu_tmp->ay_8l; // Y-axis
      _switch_command_buffer[37] = _imu_tmp->ay_8h;
      _switch_command_buffer[38] = _imu_tmp->ax_8l; // X-axis
      _switch_command_buffer[39] = _imu_tmp->ax_8h;
      _switch_command_buffer[40] = _imu_tmp->az_8l; // Z-axis
      _switch_command_buffer[41] = _imu_tmp->az_8h;
      _switch_command_buffer[42] = _imu_tmp->gy_8l;
      _switch_command_buffer[43] = _imu_tmp->gy_8h;
      _switch_command_buffer[44] = _imu_tmp->gx_8l;
      _switch_command_buffer[45] = _imu_tmp->gx_8h;
      _switch_command_buffer[46] = _imu_tmp->gz_8l;
      _switch_command_buffer[47] = _imu_tmp->gz_8h;

      // Set input data
      _switch_command_buffer[2] = input_data->right_buttons;
      _switch_command_buffer[3] = input_data->shared_buttons;
      _switch_command_buffer[4] = input_data->left_buttons;

      // Set sticks directly from hoja_analog_data
      // Saves cycles :)
      _switch_command_buffer[5] = (input_data->ls_x & 0xFF);
      _switch_command_buffer[6] = (input_data->ls_x & 0xF00) >> 8;
      // ns_input_report[7] |= (g_stick_data.lsy & 0xF) << 4;
      _switch_command_buffer[7] = (input_data->ls_y & 0xFF0) >> 4;
      _switch_command_buffer[8] = (input_data->rs_x & 0xFF);
      _switch_command_buffer[9] = (input_data->rs_x & 0xF00) >> 8;
      _switch_command_buffer[10] = (input_data->rs_y & 0xFF0) >> 4;
      _switch_command_buffer[11] = _unknown_thing();

      // printf("V: %d, %d\n", _switch_command_buffer[46], _switch_command_buffer[47]);

      tud_hid_report(_switch_command_report_id, _switch_command_buffer, 64);
      analog_send_reset();
    }
  }
}

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
