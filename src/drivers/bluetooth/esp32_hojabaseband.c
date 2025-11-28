#include "drivers/bluetooth/esp32_hojabaseband.h"

#include <string.h>
#include <stddef.h>

#include "hoja.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"
#include "hal/i2c_hal.h"

#include "devices_shared_types.h"
#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "utilities/interval.h"
#include "utilities/settings.h"

#include "usb/sinput.h"

#include "switch/switch_haptics.h"

#include "board_config.h"

#include "input/mapper.h"
#include "input/imu.h"


#if defined(HOJA_USB_MUX_DRIVER) && (HOJA_USB_MUX_DRIVER==USB_MUX_DRIVER_PI3USB4000A)
    #include "drivers/mux/pi3usb4000a.h"
#elif defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #error "HOJA_USB_MUX_DRIVER must be defined for the ESP32 bluetooth driver" 
#endif

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #include "drivers/bluetooth/esp32_hojabaseband.h"
#endif

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)

// Size of messages we send OUT
#define HOJA_I2C_MSG_SIZE_OUT 32
#define I2C_START_CMD_CRC_LEN 13

// The size of messages coming from the ESP32
#define HOJA_I2C_MSG_SIZE_IN 24
#define BT_CONNECTION_TIMEOUT_POWER_SECONDS (30)
#define BT_FLASH_BRIGHTNESS 20

#define BT_HOJABB_I2CINPUT_ADDRESS 0x76

typedef struct
{
    uint8_t mode;
    uint8_t mac[6];
} i2cinput_init_s;

typedef enum 
{
    POWER_CODE_OFF = 0, 
    POWER_CODE_RESET = 1,
    POWER_CODE_CRITICAL = 2,
} i2c_power_code_t;

// Status return data types
typedef enum
{
    I2C_STATUS_NULL, // Nothing to report
    I2C_STATUS_HAPTIC_STANDARD, // Standard haptic data
    I2C_STATUS_HAPTIC_SWITCH, // Nintendo Switch haptic data
    I2C_STATUS_FIRMWARE_VERSION, // Report fw version
    I2C_STATUS_CONNECTED_STATUS, // Connected status change
    I2C_STATUS_POWER_CODE, // Change power setting
    I2C_STATUS_MAC_UPDATE, // Update HOST save MAC address
    I2C_STATUS_HAPTIC_SINPUT, // SINPUT haptics
} i2cinput_status_t;

typedef enum
{
    I2C_CMD_STANDARD = 0xFF, // Regular input data
    I2C_CMD_MOTION = 0xFC, // Motion data
    I2C_CMD_START = 0xFE, // Launch BT Mode with parameter
    I2C_CMD_FIRMWARE_VERSION = 0xFD, // Retrieve the firmware version
} i2cinput_cmd_t;

typedef struct
{
    uint8_t cmd;
    uint16_t rand_seed; // Random data to help our CRC
    uint8_t data[19]; // Buffer for related data   
} __attribute__ ((packed)) i2cinput_status_s;

// 22 bytes
#define I2CINPUT_STATUS_SIZE sizeof(i2cinput_status_s)

typedef enum
{
    BT_CONNSTAT_NULL = -1,
    BT_CONNSTAT_DISCONNECTED = 0
} bt_connstat_t;

uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT] = {0};
uint8_t data_in[HOJA_I2C_MSG_SIZE_IN] = {0};

static bt_connstat_t _current_connected = -1;
static uint8_t _current_i2c_packet_number = 0;
bluetooth_cb_t _bluetooth_cb = NULL;

typedef struct
{
    union
    {
        struct
        {
            // D-Pad
            uint8_t dpad_up     : 1;
            uint8_t dpad_down   : 1;
            uint8_t dpad_left   : 1;
            uint8_t dpad_right  : 1;
            // Buttons
            uint8_t button_south : 1; // Switch B
            uint8_t button_east : 1; // Switch A
            uint8_t button_west : 1; // Switch Y
            uint8_t button_north : 1; // Switch X

            // Triggers
            uint8_t trigger_l   : 1;
            uint8_t trigger_zl  : 1;
            uint8_t trigger_r   : 1;
            uint8_t trigger_zr  : 1;

            // Special Functions
            uint8_t button_plus     : 1;
            uint8_t button_minus    : 1;

            // Stick clicks
            uint8_t button_stick_left   : 1;
            uint8_t button_stick_right  : 1;
        };
        uint16_t buttons_all;
    };

    union
    {
        struct
        {
            // Menu buttons (Not remappable by API)
            uint8_t button_capture  : 1;
            uint8_t button_home     : 1;
            uint8_t button_safemode : 1;
            uint8_t button_shipping : 1;
            uint8_t button_sync     : 1;
            uint8_t button_unbind   : 1;
            uint8_t trigger_gl      : 1;
            uint8_t trigger_gr      : 1;
        };
        uint8_t buttons_system;
    };

    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
    uint16_t lt;
    uint16_t rt;

    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;

    uint8_t power_stat;
} __attribute__ ((packed)) i2cinput_input_s;

#define I2CINPUT_INPUT_SIZE sizeof(i2cinput_input_s)

// Polynomial for CRC-8 (x^8 + x^2 + x + 1)
#define CRC8_POLYNOMIAL 0x07

uint8_t _crc8_compute(uint8_t *data, size_t length)
{
    uint8_t crc = 0x00; // Initial value of CRC
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i]; // XOR the next byte into the CRC

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            { // If the MSB is set
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    if (!crc)
        crc++; // Must be non-zero

    return crc;
}

bool _crc8_verify(uint8_t *data, size_t length, uint8_t received_crc)
{
    uint8_t calculated_crc = _crc8_compute(data, length);
    return calculated_crc == received_crc;
}

void _bt_clear_out()
{
    memset(data_out, 0, HOJA_I2C_MSG_SIZE_OUT);
}

void _bt_clear_in()
{
    memset(data_in, 0, HOJA_I2C_MSG_SIZE_IN);
}

void _esp32hoja_enable_chip(bool enabled)
{
    if(enabled)
    {
        gpio_hal_init(BLUETOOTH_DRIVER_ENABLE_PIN, false, true);
        gpio_hal_write(BLUETOOTH_DRIVER_ENABLE_PIN, true);
    }
    else
    {
        gpio_hal_init(BLUETOOTH_DRIVER_ENABLE_PIN, false, false);
        gpio_hal_write(BLUETOOTH_DRIVER_ENABLE_PIN, false);
    }

    if(enabled)
        // Give time for the chip to boot
        sys_hal_sleep_ms(1000);
}

void _esp32hoja_enable_uart(bool enabled)
{
    HOJA_USB_MUX_ENABLE(false);
    sys_hal_sleep_ms(100);
    if(enabled)
    {
        HOJA_USB_MUX_SELECT(1);
    }
    else
    {
        HOJA_USB_MUX_SELECT(0);
    }
    sys_hal_sleep_ms(100);
    HOJA_USB_MUX_ENABLE(true);
}

const uint32_t connection_attempt_ms = 25 * 1000;
uint32_t connection_attempts = 0;

void _btinput_message_parse(uint8_t *data)
{
    // Handle auto-shut-off timer
    if(_current_connected < 1)
    {
        connection_attempts++;
        if(connection_attempts >= connection_attempt_ms)
        {
            connection_attempts = 0;
            // Shut down
        }
    }

    uint8_t crc = data[0];
    uint8_t counter = data[1];
    bool packet_updated = false;
    i2cinput_status_s status = {0};

    bluetooth_cb_msg_s btcb_msg = {0};

    const uint8_t attempts_reset = 25;
    static uint8_t attempts_remaining = attempts_reset;

    // Verify CRC before proceeding (only if it's present)
    if (crc > 0)
    {
        bool verified = false;
        verified = _crc8_verify((uint8_t *)&(data[2]), sizeof(i2cinput_status_s), crc);

        if (!verified)
        {
            return;
        }

        memcpy(&status, &(data[2]), sizeof(i2cinput_status_s));
            
        if(_current_i2c_packet_number != counter)
        {
            packet_updated = true;
            _current_i2c_packet_number = counter;
        }
    }   

    // Only process fresh data
    if(!packet_updated)
    {
        return;
    }

    switch (status.cmd)
    {
    default:
        break;

    // No data to report
    case I2C_STATUS_NULL:
    {
    }
    break;

    case I2C_STATUS_CONNECTED_STATUS:
    {
        // Handle connected status
        if (_current_connected != status.data[0])
        {
            _current_connected = (int8_t) status.data[0];
            if (_current_connected > 0)
            {
                // Connected OK
                hoja_set_connected_status(_current_connected);
            }
            else
            {
                // Disconnected
                hoja_set_connected_status(CONN_STATUS_DISCONNECTED);
            }
        }
    }
    break;

    case I2C_STATUS_POWER_CODE:
    {
        uint8_t power_code = status.data[0];

        btcb_msg.type = BTCB_POWER_CODE;

        if (power_code == POWER_CODE_OFF)
        {
            if(_bluetooth_cb)
            {
                btcb_msg.data[0] = 0;
                _bluetooth_cb(&btcb_msg);
            }
        }
        else if (power_code == POWER_CODE_RESET)
        {
            if(_bluetooth_cb)
            {
                btcb_msg.data[0] = 1;
                _bluetooth_cb(&btcb_msg);
            }
        }
        else if(power_code == POWER_CODE_CRITICAL)
        {
            if(_bluetooth_cb)
            {
                btcb_msg.data[0] = 2;
                _bluetooth_cb(&btcb_msg);
            }
        }
    }
    break;

    case I2C_STATUS_HAPTIC_SWITCH:
    {
        switch_haptics_rumble_translate(&(status.data[0]));
    }
    break;

    case I2C_STATUS_HAPTIC_STANDARD:
    {
        uint8_t l = status.data[0];
        uint8_t r = status.data[1];
        haptics_set_std(l>r? l : r, false);
    }
    break;

    case I2C_STATUS_HAPTIC_SINPUT:
    {
        sinput_cmd_haptics(&(status.data[0]));
    }
    break;
    }

}

// Init firmware loading mode
void esp32hoja_init_load() 
{
    #if defined(HOJA_USB_MUX_INIT)
    HOJA_USB_MUX_INIT();
    #endif

    _esp32hoja_enable_uart(true);
    sys_hal_sleep_ms(200);
    _esp32hoja_enable_chip(true);
}

bool esp32hoja_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb)
{
    #if defined(HOJA_USB_MUX_INIT)
    HOJA_USB_MUX_INIT();
    #endif

    if(evt_cb)
        _bluetooth_cb = evt_cb;

    uint8_t pair_flag = (pairing_mode ? 0b10000000 : 0);
    uint8_t out_mode = pair_flag | (uint8_t) device_mode;

    #if defined(HOJA_BT_LOGGING_DEBUG) && (HOJA_BT_LOGGING_DEBUG==1)
    _esp32hoja_enable_uart(true);
    #endif

    _esp32hoja_enable_chip(true);

    #if defined(HOJA_BT_LOGGING_DEBUG) && (HOJA_BT_LOGGING_DEBUG==1)
    sys_hal_sleep_ms(5000);
    #endif

    _bt_clear_out();

    data_out[0] = I2C_CMD_START;
    data_out[2] = out_mode;

    #if defined(BLUETOOTH_DRIVER_BATMON_ENABLE) && (BLUETOOTH_DRIVER_BATMON_ENABLE==1)
        #if !defined(BLUETOOTH_DRIVER_BATMON_ADC_GPIO)
        #error "BLUETOOTH_DRIVER_BATMON_ADC_GPIO must be defined if BLUETOOTH_DRIVER_BATMON_ENABLE==1"
        #else 
        data_out[3] = true;
        data_out[4] = BLUETOOTH_DRIVER_BATMON_ADC_GPIO;
        #endif
    #else 
        data_out[3] = false;
        data_out[4] = 0;
    #endif

    // RGB Data
    #define GET_RED(val) ((val & 0xFF0000) >> 16)
    #define GET_GREEN(val) ((val & 0xFF00) >> 8)
    #define GET_BLUE(val) ((val & 0xFF))
    uint32_t col;

    col = gamepad_config->gamepad_color_grip_left;
    data_out[5] = GET_RED(col);
    data_out[6] = GET_GREEN(col);
    data_out[7] = GET_BLUE(col);

    col = gamepad_config->gamepad_color_grip_right;
    data_out[8] = GET_RED(col);
    data_out[9] = GET_GREEN(col);
    data_out[10] = GET_BLUE(col);

    col = gamepad_config->gamepad_color_body;
    data_out[11] = GET_RED(col);
    data_out[12] = GET_GREEN(col);
    data_out[13] = GET_BLUE(col);

    col = gamepad_config->gamepad_color_buttons;
    data_out[14] = GET_RED(col);
    data_out[15] = GET_GREEN(col);
    data_out[16] = GET_BLUE(col);

    // New VID/PID section for SInput mode
    uint16_t vid = 0;
    uint16_t pid = 0;

    #if defined(HOJA_USB_VID)
    vid = HOJA_USB_VID;
    #else
    vid = 0x2E8A; // Raspberry Pi
    #endif
    
    #if defined(HOJA_USB_PID)
    pid = HOJA_USB_PID; // board_config PID
    #else
    pid = 0x10C6; // Hoja Gamepad
    #endif

    data_out[17] = (vid >> 8);
    data_out[18] = (vid & 0xFF);

    data_out[19] = (pid >> 8);
    data_out[20] = (pid & 0xFF);
    // end VID/PID section

    // Stupid workaround for SuperGamepad+ :)
    #if defined(HOJA_SINPUT_GAMEPAD_SUBTYPE)
    data_out[21] = HOJA_SINPUT_GAMEPAD_SUBTYPE;
    #endif

    // Local MAC
    data_out[22] = gamepad_config->switch_mac_address[0];
    data_out[23] = gamepad_config->switch_mac_address[1];
    data_out[24] = gamepad_config->switch_mac_address[2];
    data_out[25] = gamepad_config->switch_mac_address[3];
    data_out[26] = gamepad_config->switch_mac_address[4];
    data_out[27] = gamepad_config->switch_mac_address[5] + (uint8_t) hoja_get_status().gamepad_mode;

    // Calculate CRC
    uint8_t crc = _crc8_compute(&(data_out[2]), I2C_START_CMD_CRC_LEN);
    data_out[1] = crc;

    int stat = i2c_hal_write_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 150000);

    if (stat < 0)
    {
        return false;
    }
    else return true;
}

void esp32hoja_stop()
{
    _esp32hoja_enable_uart(false);
    _esp32hoja_enable_chip(false);
}

void esp32hoja_task(uint64_t timestamp)
{
    static interval_s       interval    = {0};
    static i2cinput_input_s input_data  = {0};
    
    static bool read_write = true;

    if(interval_run(timestamp, 650, &interval))
    {
        memset(data_out, 0x80, 24); // Reset buffer to 0x80 for blank

        if(read_write)
        {
            mapper_input_s *input = mapper_get_input();

            data_out[0] = I2C_CMD_STANDARD;
            data_out[1] = 0;                  // Input CRC location
            data_out[2] = _current_i2c_packet_number; // Response packet number counter

            // input_data.buttons_all      = 0;//buttons.buttons_all;
            // input_data.buttons_system   = 0;//buttons.buttons_system;

            //input_data.lx = (uint16_t) (input->joysticks_combined[0] + 2048);
            //input_data.ly = (uint16_t) (input->joysticks_combined[1] + 2048);
            //input_data.rx = (uint16_t) (input->joysticks_combined[2] + 2048);
            //input_data.ry = (uint16_t) (input->joysticks_combined[3] + 2048);

            // Clamp values between 0 and 4095
            //input_data.lx = (input_data.lx > 4095) ? 4095 : input_data.lx;
            //input_data.ly = (input_data.ly > 4095) ? 4095 : input_data.ly;
            //input_data.rx = (input_data.rx > 4095) ? 4095 : input_data.rx;
            //input_data.ry = (input_data.ry > 4095) ? 4095 : input_data.ry;

            

            switch(hoja_get_status().gamepad_mode)
            {
                default:
                case GAMEPAD_MODE_SWPRO:
                /*
                input_data.button_south = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_B);
                input_data.button_east = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_A);
                input_data.button_west = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_Y);
                input_data.button_north = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_X);

                input_data.dpad_down     = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_DOWN);
                input_data.dpad_right    = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_RIGHT);
                input_data.dpad_left     = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_LEFT);
                input_data.dpad_up       = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_UP);

                input_data.button_minus    = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_MINUS);
                input_data.button_plus     = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_PLUS);
                input_data.button_home     = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_HOME);
                input_data.button_capture  = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_CAPTURE);

                input_data.button_stick_left   = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_LS);
                input_data.button_stick_right    = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_RS);

                input_data.trigger_r = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_R);
                input_data.trigger_l = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_L);
                
                input_data.trigger_zl = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_LZ);
                input_data.trigger_zr = MAPPER_BUTTON_DOWN(input->digital_inputs, SWITCH_CODE_RZ);

                input_data.lt = (uint16_t) input->triggers[0];
                input_data.rt = (uint16_t) input->triggers[1];*/
                break;
                case GAMEPAD_MODE_SINPUT:
                /*
                input_data.button_south = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_SOUTH);
                input_data.button_east = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_EAST);
                input_data.button_west = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_WEST);
                input_data.button_north = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_NORTH);

                input_data.dpad_down     = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_DOWN);
                input_data.dpad_right    = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RIGHT);
                input_data.dpad_left     = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LEFT);
                input_data.dpad_up       = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_UP);

                input_data.button_minus    = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_SELECT);
                input_data.button_plus     = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_START);
                input_data.button_home     = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_HOME);
                input_data.button_capture  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_CAPTURE);

                input_data.button_stick_left   = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LS);
                input_data.button_stick_right    = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RS);

                input_data.trigger_r = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RB);
                input_data.trigger_l = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LB);

                bool l_digital = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LT);
                bool r_digital = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RT);

                #if defined(HOJA_ADC_LT_CFG)
                if(trigger_config->left_disabled == 1)
                {
                    if(l_digital)
                        input_data.lt = 0xFFF;
                    else 
                        input_data.lt = 0;
                }
                else 
                #endif
                {
                    input_data.lt = (uint16_t) input->triggers[0];
                    input_data.trigger_zl = l_digital;
                }

                #if defined(HOJA_ADC_RT_CFG)
                if(trigger_config->right_disabled == 1)
                {
                    if(r_digital)
                        input_data.rt = 0xFFF;
                    else 
                        input_data.rt = 0;
                }
                else 
                #endif
                {
                    input_data.rt = (uint16_t) input->triggers[1];
                    input_data.trigger_zr = r_digital;
                }
                    */
                break;
            }

            static imu_data_s imu = {0};
            imu_access_safe(&imu);

            input_data.gx = imu.gx;
            input_data.gy = imu.gy;
            input_data.gz = imu.gz;
            input_data.ax = imu.ax;
            input_data.ay = imu.ay;
            input_data.az = imu.az;

            // Power status
            {
                static battery_status_s bstat = {0};
                static fuelgauge_status_s fstat = {0};

                battery_get_status(&bstat);
                fuelgauge_get_status(&fstat);

                esp32_battery_status_u s = {
                    .bat_lvl    = 4,
                    .charging   = 0,
                    .connection = 0
                };

                if(bstat.connected)
                {
                    if(bstat.charging)
                    {
                        s.charging = 1;
                    }
                    else s.charging = 0;
                }

                if(fstat.connected)
                {
                    switch(fstat.simple)
                    {
                        case BATTERY_LEVEL_CRITICAL:
                        s.bat_lvl = 1;
                        break;

                        case BATTERY_LEVEL_LOW:
                        s.bat_lvl = 1;
                        break;

                        case BATTERY_LEVEL_MID:
                        s.bat_lvl = 2;
                        break;

                        case BATTERY_LEVEL_HIGH:
                        s.bat_lvl = 4;
                        break;
                    }
                }

                input_data.power_stat = s.val;
            }

            uint8_t crc = _crc8_compute((uint8_t *)&input_data, sizeof(i2cinput_input_s)-1); // Subtract 1 for size because of legacy compatibility for baseband
            data_out[1] = crc;

            memcpy(&(data_out[3]), &input_data, sizeof(i2cinput_input_s));

            int write = i2c_hal_write_timeout_us_odbaud(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 32000, 800);
            if (write == HOJA_I2C_MSG_SIZE_OUT)
            {
                _bt_clear_out();
            }

        }  
        else
        {
            int read = i2c_hal_read_timeout_us_odbaud(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 32000, 800);
            
            if (read == HOJA_I2C_MSG_SIZE_IN)
            {
                _btinput_message_parse(data_in);
                _bt_clear_in();
            }
        }
        read_write = !read_write;
    }
}

int esp32hoja_hwtest()
{

}

#define BTINPUT_GET_VERSION_ATTEMPTS 10

uint32_t esp32hoja_get_fwversion()
{
    uint32_t ret_info = 0x00;

    uint8_t attempts = BTINPUT_GET_VERSION_ATTEMPTS;

    _esp32hoja_enable_chip(true);

    static uint8_t data_in[HOJA_I2C_MSG_SIZE_IN] = {0};

    while (attempts--)
    {
        _bt_clear_out();
        data_out[0] = I2C_CMD_FIRMWARE_VERSION;

        int stat = i2c_hal_write_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 10000);
        sys_hal_sleep_ms(4);
        int read = i2c_hal_read_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 10000);

        if (read == HOJA_I2C_MSG_SIZE_IN)
        {
            uint16_t version = (data_in[1] << 8) | (data_in[2]);
            ret_info |= version;

            _esp32hoja_enable_chip(false);
            return ret_info;
        }
    }

    _esp32hoja_enable_chip(false);
    return 0x00;
}

#endif