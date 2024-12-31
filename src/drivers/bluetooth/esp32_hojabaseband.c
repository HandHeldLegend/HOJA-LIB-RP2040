#include "drivers/bluetooth/esp32_hojabaseband.h"

#include <string.h>
#include <stddef.h>

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"
#include "hal/i2c_hal.h"

#include "utilities/interval.h"

#include "switch/switch_haptics.h"

#include "input/button.h"
#include "input/analog.h"
#include "input/imu.h"

#if defined(HOJA_USB_MUX_DRIVER) && (HOJA_USB_MUX_DRIVER==USB_MUX_DRIVER_PI3USB4000A)
    #include "drivers/mux/pi3usb4000a.h"
#elif defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #error "HOJA_USB_MUX_DRIVER must be defined for the ESP32 bluetooth driver" 
#endif

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #include "drivers/bluetooth/esp32_hojabaseband.h"
#endif

// Size of messages we send OUT
#define HOJA_I2C_MSG_SIZE_OUT 32

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
    uint32_t rand_seed; // Random data to help our CRC
    uint8_t data[10]; // Buffer for related data
} __attribute__ ((packed)) i2cinput_status_s;

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
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;

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
            uint8_t padding         : 5;
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
} __attribute__ ((packed)) i2cinput_input_s;

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
        gpio_hal_write(BLUETOOTH_DRIVER_ENABLE_PIN, true);
        gpio_hal_set_direction(BLUETOOTH_DRIVER_ENABLE_PIN, true);
    }
    else
    {
        gpio_hal_set_direction(BLUETOOTH_DRIVER_ENABLE_PIN, false);
        gpio_hal_write(BLUETOOTH_DRIVER_ENABLE_PIN, false);
    }
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
            }
            else
            {
                // Disconnected
            }
        }
    }
    break;

    case I2C_STATUS_POWER_CODE:
    {
        uint8_t power_code = status.data[0];

        if (power_code == 0)
        {
            // Shut down
        }
        else if (power_code == 1)
        {
            // Reboot
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
    }
    break;

    case I2C_STATUS_MAC_UPDATE:
    {
        // Paired to Nintendo Switch
        //printf("New BT Switch Pairing Completed.");
        //global_loaded_settings.switch_host_address[0] = status.data[0];
        //global_loaded_settings.switch_host_address[1] = status.data[1];
        //global_loaded_settings.switch_host_address[2] = status.data[2];
        //global_loaded_settings.switch_host_address[3] = status.data[3];
        //global_loaded_settings.switch_host_address[4] = status.data[4];
        //global_loaded_settings.switch_host_address[5] = status.data[5];
        //settings_save_from_core0();
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

    _esp32hoja_enable_chip(true);
    // Give time for the chip to boot
    sys_hal_sleep_ms(1000);

    _bt_clear_out();

    data_out[0] = I2C_CMD_START;
    data_out[2] = out_mode;

    // Calculate CRC
    uint8_t crc = _crc8_compute(&(data_out[2]), 13);
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

void esp32hoja_task(uint32_t timestamp)
{
    static interval_s       interval    = {0};
    static i2cinput_input_s input_data  = {0};
    static bool read_write = true;

    if(interval_run(timestamp, 1000, &interval))
    {
        memset(data_out, 0x80, 24); // Reset buffer to 0x80 for blank

        if(read_write)
        {
            // Update input states
            static button_data_s   buttons = {0};
            static analog_data_s   analog  = {0};
            static imu_data_s      imu_tmp    = {0};
            button_access_try(&buttons, BUTTON_ACCESS_REMAPPED_DATA);
            analog_access_try(&analog,  ANALOG_ACCESS_DEADZONE_DATA);
        
            data_out[0] = I2C_CMD_STANDARD;
            data_out[1] = 0;                  // Input CRC location
            data_out[2] = _current_i2c_packet_number; // Response packet number counter

            input_data.buttons_all = buttons.buttons_all;
            input_data.buttons_system = buttons.buttons_system;

            input_data.lx = (uint16_t) analog.lx;
            input_data.ly = (uint16_t) analog.ly;
            input_data.rx = (uint16_t) analog.rx;
            input_data.ry = (uint16_t) analog.ry;

            input_data.lt = (uint16_t) buttons.zl_analog;
            input_data.rt = (uint16_t) buttons.zr_analog;

            imu_access_try(&imu_tmp);

            input_data.gx = imu_tmp.gx;
            input_data.gy = imu_tmp.gy;
            input_data.gz = imu_tmp.gz;
            input_data.ax = imu_tmp.ax;
            input_data.ay = imu_tmp.ay;
            input_data.az = imu_tmp.az;

            uint8_t crc = _crc8_compute((uint8_t *)&input_data, sizeof(i2cinput_input_s));
            data_out[1] = crc;

            memcpy(&(data_out[3]), &input_data, sizeof(i2cinput_input_s));

            int write = i2c_hal_write_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 16000);
            if (write == HOJA_I2C_MSG_SIZE_OUT)
            {
                _bt_clear_out();
            }

        }  
        else
        {
            int read = i2c_hal_read_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, BT_HOJABB_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 8000);
            
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

uint32_t esp32hoja_get_info()
{
    uint16_t driver_type = 0xF000;
    uint32_t ret_info = (driver_type<<16);

    uint8_t attempts = BTINPUT_GET_VERSION_ATTEMPTS;

    // 0xFFFF indicates that the firmware is unused
    uint16_t v = 0x0001;

    v = 0xFFFE;

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
    return ret_info;
}