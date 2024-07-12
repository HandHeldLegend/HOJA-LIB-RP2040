#include "switch_commands.h"

// This C file handles various Switch gamepad commands (OUT reports)
uint8_t _switch_in_command_buffer[64] = {0};
uint8_t _switch_in_report_id = 0x00;
uint16_t _switch_in_command_len = 64;
bool _switch_in_command_got = false;

uint8_t _switch_reporting_mode = 0x3F;
uint8_t _switch_imu_mode = 0x00;

uint8_t _switch_command_buffer[64] = {0};
uint8_t _switch_command_report_id = 0x00;
// uint8_t _switch_mac_address[6] = {0};
uint8_t _switch_ltk[16] = {0};

void generate_ltk()
{
  //printf("Generated LTK: ");
  for (uint8_t i = 0; i < 16; i++)
  {
    _switch_ltk[i] = get_rand_32() & 0xFF;
    //printf("%X : ", _switch_ltk[i]);
  }
  //printf("\n");
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
  static uint32_t last_time = 0;
  uint32_t this_time = hoja_get_timestamp();

  double diff = (double) abs((int) this_time - (int) last_time);
  last_time = this_time;
  // Calculate time from us to ms
  diff /= 1000;
  uint32_t u32diff = (uint32_t) diff;

  static int16_t _switch_timer = 0;
  _switch_command_buffer[0] = (uint8_t)_switch_timer;
  // //printf("Td=%d \n", _switch_timer);
  _switch_timer += u32diff;
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
  // New firmware causes issue with gyro needs more research
  _switch_command_buffer[14] = 0x04; // NS Firmware primary   (4.x)
  _switch_command_buffer[15] = 0x33; // NS Firmware secondary (x.21) 

  //_switch_command_buffer[14] = 0x03; // NS Firmware primary   (3.x)
  //_switch_command_buffer[15] = 72;   // NS Firmware secondary (x.72)

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

// 40-625hz low frequency
// 585hz range
float _get_low_frequency(uint16_t value)
{
  value = (value > 127) ? 127 : value;
  const float scale = 4.60629f;

  if(!value) return 40.0f;
  else return ((float) value * scale) + 40.0f;
}

// 80-1250hz high frequency
// 1170hz range
float _get_high_frequency(uint16_t value)
{
  value = (value > 127) ? 127 : value;
  const float scale = 9.21259f;

  if(!value) return 80.0f;
  else return ((float) value * scale) + 80.0f;
}

float _get_amplitude(uint16_t value)
{
  value = (value > 127) ? 127 : value;
  if(!value) return 0;

  return (float) value/127.0f;
}

// Translate and handle rumble
// Big thanks to hexkyz for some info on Discord
void rumble_translate(const uint8_t *data)
{
  // Extract modulation indicator in byte 3
  // v9 / result
  uint8_t upper2 = data[3] >> 6;
  int result = (upper2)>0;

  uint16_t hfcode = 0;
  uint16_t lacode = 0;
  uint16_t lfcode = 0;
  uint16_t hacode = 0;

  float fhi = 0;
  float ahi = 0;

  float flo = 0;
  float alo = 0;

  // Single wave w/ resonance
  // v14
  bool high_f_select = false;
  
  // v4
  uint8_t patternType = 0x00;

    // Handle different modulation types
    if (upper2) // If upper 2 bits exist
    {
        if (upper2 != 1) 
        {
            if (upper2 == 2) // Value is 2
            {
                // Check if lower 10 bytes of first two bytes exists
                uint16_t lower10 = (data[1]<<8) | data[0];
                if (lower10 & 0x03FF)
                {
                    patternType = 5;
                }
                else patternType = 2;
            } 
            else if (upper2 == 3) // Value is 3
            {
                patternType = 3;
            }
            
            goto LABEL_24;
        }
        
        // Upper 2 bits are unset...
        // The format of the rumble is treated differently.
        uint16_t lower12 = (data[2]<<8 | data[1]);
        
        if (!(lower12 & 0x0FFF)) // if lower 12 bits are empty
        {
            patternType = 1;
            goto LABEL_24;
        }
        
        // Lower 12 bits are set
        // Format is different again
        
        // Check lower 2 bits of byte 0
        // v10
        uint16_t lower2 = data[0] & 0x3;
        
        if (!lower2) // Lower 2 is blank (0x00)
        {
            patternType = 4;
            goto LABEL_24;
        }
        if ((lower2 & 2) != 0) // Lower 2, bit 1 is set (0x02 or 0x03)
        {
            result = 3;
            patternType = 7;
            goto LABEL_24;
        }
        
        // Lower 2, bit 0 is set only (0x01)
        // Do nothing?
        return;
    }
    
    // Check if there is no int value present in the data
    
    //if (!(4 * *(int *)data)) {
    //    return;
    //}
    //printf("Int found\n");

    result = 3;
    patternType = 6;

  LABEL_24:

  // Unpack codes based on modulation type
  switch (patternType) {
      case 0:
      case 1:
      case 2:
      case 3:
          //printf("Case 0-3\n");
          /*
          amFmCodes[0] = (v9 >> 1) & 0x1F;
          amFmCodes[1] = (*((unsigned short *)data + 1) >> 4) & 0x1F;
          amFmCodes[2] = (*(unsigned short *)(data + 1) >> 7) & 0x1F;
          amFmCodes[3] = (data[1] >> 2) & 0x1F;
          amFmCodes[4] = (*(unsigned short *)data >> 5) & 0x1F;
          v12 = *data & 0x1F;
          */
          lacode = 0;
          lfcode = 0;
          hacode = 0;
          hfcode = 0;
          break;

      // Dual frequency mode
      case 4:
          //printf("Case 4\n");
          
          // Low channel
          lfcode = (data[2]&0x7F); // Low Frequency

          lacode = ((data[3]&0x3F)<<1) | ((data[2]&0x80)>>7); // Low Amplitude
          
          // 40-625hz
          //printf("LF : %x\n", lfcode);
          //printf("LA : %x\n", lacode);
          
          // High channel
          hfcode = ((data[1] & 0x1)<<7) | (data[0] >> 2);
          hacode = (data[1]>>1);
          
          // 80-1250hz
          //printf("HF : %x\n", hfcode);
          //printf("HA : %x\n", hacode);
          break;

      // Seems to be single wave mode
      case 5:
      case 6:
          //printf("Case 5-6\n");
          
          // Byte 0 is frequency and high/low select bit
          // check byte 0 bit 0
          // 1 indicates high channel
          high_f_select = data[0] & 1;

          if (high_f_select)
          {
              //printf("HF Bit ON\n");
              
              hfcode = (data[0]>>1);
              hacode = (data[1] & 0xF) << 3;
              
              //printf("LF is 160hz.");
              lfcode = 0;
              lacode = ( ((data[2] & 0x1)<<3) | ( (data[1]&0xE0)>>5 ) ) << 3;
              flo = 160.0f;
          }
              
          else
          {
              //printf("LF Bit ON\n");
              
              lfcode = (data[0]>>1);
              hacode = (data[1] & 0xF) << 3;
              
              //printf("HF is 320hz.");
              hfcode = 0;
              lacode = ( ((data[2] & 0x1)<<3) | ( (data[1]&0xE0)>>5 ) ) << 3;
              fhi = 320.0f;
          }
          
          if(data[1] & 0x10)
          {
              // Hi freqency amplitude disable
              hacode = 0;
          }
          
          if(data[2] & 0x2)
          {
              // Lo frequency amplitude disable
              lacode = 0;
          }
          
          // 80-1250hz
          //printf("HF : %x\n", hfcode);
          //printf("HA : %x\n", hacode);
          
          //printf("LF : %x\n", lfcode);
          //printf("LA : %x\n", lacode);
          break;

      // Some kind of operation codes? Also contains frequency
      case 7:
          //printf("Case 7\n");
          /*
          v18 = *data;
          v19 = v18 & 1;
          v20 = ((v18 >> 2) & 1) == 0;
          v21 = (*((unsigned short *)data + 1) >> 7) & 0x7F;

          if (v20)
              v22 = v21 | 0x80;
          else
              v22 = (v21 << 8) | 0x8000;

          if (v19)
              v23 = 24;
          else
              v23 = v22;
          amFmCodes[0] = v23;
          if (!v19)
              v22 = 24;
          amFmCodes[1] = v22;
          amFmCodes[2] = (data[2] >> 2) & 0x1F;
          amFmCodes[3] = (*(unsigned short *)(data + 1) >> 5) & 0x1F;
          amFmCodes[4] = data[1] & 0x1F;
          v12 = *data >> 3;*/
        return;
        break;

      default:
          break;
  }

  if(!fhi)
  fhi = _get_high_frequency(hfcode);

  if(!flo)
  flo = _get_low_frequency(lfcode);

  ahi = _get_amplitude(hacode);
  alo = _get_amplitude(lacode);

  hoja_rumble_set(fhi, ahi, flo, alo);
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
    //printf("MAC Address requested.");
    info_set_mac();
    break;

  default:
    //printf("Unknown setup requested: %X", info_code);
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
  //printf("CMD: ");

  switch (command)
  {
  case SW_CMD_SET_NFC:
    //printf("Set NFC MCU:\n");
    set_ack(0x80);
    break;

  case SW_CMD_ENABLE_IMU:
    //printf("Enable IMU: %d\n", data[11]);
    imu_set_enabled(data[11] > 0);
    _switch_imu_mode = data[11];
    set_ack(0x80);
    break;

  case SW_CMD_SET_PAIRING:
    //printf("Set pairing.\n");
    pairing_set(data[11], &data[12]);
    break;

  case SW_CMD_SET_INPUTMODE:
    //printf("Input mode change: %X\n", data[11]);
    set_ack(0x80);
    _switch_reporting_mode = data[11];
    break;

  case SW_CMD_GET_DEVICEINFO:
    //printf("Get device info.\n");
    set_ack(0x82);
    set_devinfo();
    break;

  case SW_CMD_SET_SHIPMODE:
    //printf("Set ship mode: %X\n", data[11]);
    set_ack(0x80);
    break;

  case SW_CMD_GET_SPI:
    //printf("Read SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
    set_ack(0x90);
    sw_spi_readfromaddress(data[12], data[11], data[15]);
    break;

  case SW_CMD_SET_HCI:
    // For now all options should shut down
    util_battery_enable_ship_mode();
    break;

  case SW_CMD_SET_SPI:
    //printf("Write SPI. Address: %X, %X | Len: %d\n", data[12], data[11], data[15]);
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
    //printf("Get trigger ET.\n");
    set_ack(0x83);
    set_sub_triggertime(100);
    break;

  case SW_CMD_ENABLE_VIBRATE:
    //printf("Enable vibration.\n");
    set_ack(0x80);
    break;

  case SW_CMD_SET_PLAYER:
    //printf("Set player: ");
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
    rgb_set_player(set_num);
    break;

  default:
    //printf("Unhandled: %X\n", command);
    for (uint16_t i = 0; i < len; i++)
    {
      //printf("%X, ", data[i]);
    }
    //printf("\n");
    set_ack(0x80);
    break;
  }

  //printf("Sent: ");
  for (uint8_t i = 0; i < 32; i++)
  {
    //printf("%X, ", _switch_command_buffer[i]);
  }
  //printf("\n");

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
    //printf("Unknown report: %X\n", report_id);
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

      if(_switch_imu_mode == 0x01)
      {
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
      }
      else if(_switch_imu_mode == 0x02)
      {
        // New Gyro test code
        static mode_2_s mode_2_data = {0};

        imu_pack_quat(&mode_2_data);

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
      // ns_input_report[7] |= (g_stick_data.lsy & 0xF) << 4;
      _switch_command_buffer[7] = (input_data->ls_y & 0xFF0) >> 4;
      _switch_command_buffer[8] = (input_data->rs_x & 0xFF);
      _switch_command_buffer[9] = (input_data->rs_x & 0xF00) >> 8;
      _switch_command_buffer[10] = (input_data->rs_y & 0xFF0) >> 4;
      _switch_command_buffer[11] = _unknown_thing();

      // //printf("V: %d, %d\n", _switch_command_buffer[46], _switch_command_buffer[47]);

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
