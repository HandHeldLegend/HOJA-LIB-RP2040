#include "btinput.h"
#include "interval.h"

// Size of messages we send OUT
#define HOJA_I2C_MSG_SIZE_OUT 32

// The size of messages coming from the ESP32
#define HOJA_I2C_MSG_SIZE_IN 24
#define BT_CONNECTION_TIMEOUT_POWER_SECONDS (30)
#define BT_FLASH_BRIGHTNESS 20

#if (HOJA_CAPABILITY_BLUETOOTH == 1)
uint32_t _mode_color = 0;
uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT] = {0};
uint8_t data_in[HOJA_I2C_MSG_SIZE_IN] = {0};



// This flag can be reset to tell the web app
// that the controller is bluetooth capable
bool _bt_capability_reset_flag = false;
#endif

// Polynomial for CRC-8 (x^8 + x^2 + x + 1)
#define CRC8_POLYNOMIAL 0x07

uint8_t crc8_compute(uint8_t *data, size_t length)
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

bool crc8_verify(uint8_t *data, size_t length, uint8_t received_crc)
{
    uint8_t calculated_crc = crc8_compute(data, length);
    return calculated_crc == received_crc;
}

#if (HOJA_CAPABILITY_BLUETOOTH == 1)
void _bt_clear_out()
{
    memset(data_out, 0, HOJA_I2C_MSG_SIZE_OUT);
}

void _bt_clear_in()
{
    memset(data_in, 0, HOJA_I2C_MSG_SIZE_IN);
}
#endif

void btinput_capability_reset_flag()
{
#if (HOJA_CAPABILITY_BLUETOOTH == 1)
    _bt_capability_reset_flag = true;
#endif
}

#define BTINPUT_GET_VERSION_ATTEMPTS 10
uint16_t btinput_get_version()
{
    uint8_t attempts = BTINPUT_GET_VERSION_ATTEMPTS;

    // 0xFFFF indicates that the firmware is unused
    uint16_t v = 0x0001;
#if (HOJA_CAPABILITY_BLUETOOTH == 1)

#ifdef HOJA_CAPABILITY_BLUETOOTH_OPTIONAL
#if (HOJA_CAPABILITY_BLUETOOTH_OPTIONAL == 1)
    // Only set this if our flag is reset
    if (_bt_capability_reset_flag)
#endif
#endif
    {
        // 0xFFFE indicates that an ESP32 is present, but not initialized
        v = 0xFFFE;
        _bt_capability_reset_flag = false;
    }

    cb_hoja_set_bluetooth_enabled(true);
    sleep_ms(650);

    static uint8_t data_in[HOJA_I2C_MSG_SIZE_IN] = {0};

    while (attempts--)
    {
        _bt_clear_out();
        data_out[0] = I2C_CMD_FIRMWARE_VERSION;

        int stat = i2c_safe_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 10000);
        sleep_ms(4);
        int read = i2c_safe_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 10000);

        if (read == HOJA_I2C_MSG_SIZE_IN)
        {
            uint32_t version = (data_in[1] << 8) | (data_in[2]);
            return version;
        }
    }

    cb_hoja_set_bluetooth_enabled(false);
#endif
    return v;
}

bool btinput_init(input_mode_t input_mode)
{
#if (HOJA_CAPABILITY_BLUETOOTH == 1)
#ifdef HOJA_BT_LOGGING_DEBUG
#if (HOJA_BT_LOGGING_DEBUG == 1)
    cb_hoja_set_uart_enabled(true);
#endif
#endif

    uint8_t mode = input_mode & 0b1111111;

    switch (mode)
    {
    case INPUT_MODE_SWPRO:
        _mode_color = COLOR_WHITE.color;
        break;

    case INPUT_MODE_GCUSB:
        _mode_color = COLOR_CYAN.color;
        break;

    case INPUT_MODE_XINPUT:
        _mode_color = COLOR_GREEN.color;
        break;

    case INPUT_MODE_BASEBANDUPDATE:
        _mode_color = COLOR_ORANGE.color;
        break;

    default:
        input_mode = INPUT_MODE_SWPRO;
        _mode_color = COLOR_WHITE.color;
        break;
    }
    rgb_flash(_mode_color, -1);
    cb_hoja_set_bluetooth_enabled(true);

    // BT Baseband update
    if (mode == INPUT_MODE_BASEBANDUPDATE)
    {
        cb_hoja_set_uart_enabled(true);
        hoja_set_baseband_update(true);
        return true;
    }

// Optional delay to ensure you have time to hook into the UART
#ifdef HOJA_BT_LOGGING_DEBUG
#if (HOJA_BT_LOGGING_DEBUG == 1)
    sleep_ms(5000);
#endif
#endif

    sleep_ms(1000);

    _bt_clear_out();

    data_out[0] = I2C_CMD_START;

    data_out[2] = (uint8_t) input_mode;

    //data_out[3] = global_loaded_settings.switch_mac_address[0];
    //data_out[4] = global_loaded_settings.switch_mac_address[1];
    //data_out[5] = global_loaded_settings.switch_mac_address[2];
    //data_out[6] = global_loaded_settings.switch_mac_address[3];
    //data_out[7] = global_loaded_settings.switch_mac_address[4];
    //data_out[8] = global_loaded_settings.switch_mac_address[5];
    //data_out[9] = global_loaded_settings.switch_host_address[0];
    //data_out[10] = global_loaded_settings.switch_host_address[1];
    //data_out[11] = global_loaded_settings.switch_host_address[2];
    //data_out[12] = global_loaded_settings.switch_host_address[3];
    //data_out[13] = global_loaded_settings.switch_host_address[4];
    //data_out[14] = global_loaded_settings.switch_host_address[5];

    // Calculate CRC
    uint8_t crc = crc8_compute(&(data_out[2]), 13);
    data_out[1] = crc;

    int stat = i2c_safe_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 150000);

    if (stat < 0)
    {
        rgb_flash(COLOR_RED.color, -1);
        return false;
    }

    imu_set_enabled(true);
    return true;
#else
    return false;
#endif
}

#define I2C_MSG_STATUS_IDX 4
#define I2C_MSG_DATA_START 5
#define I2C_MSG_CMD_IDX 3

const uint32_t connection_attempt_ms = 25 * 1000;
uint32_t connection_attempts = 0;

typedef enum
{
    BT_CONNSTAT_NULL = -1,
    BT_CONNSTAT_DISCONNECTED = 0
} bt_connstat_t;

static bt_connstat_t _current_connected = -1;
static uint8_t _current_i2c_packet_number = 0;

void _btinput_message_parse(uint8_t *data)
{
#if (HOJA_CAPABILITY_BLUETOOTH == 1)

    // Handle auto-shut-off timer
    if(_current_connected < 1)
    {
        connection_attempts++;
        if(connection_attempts >= connection_attempt_ms)
        {
            connection_attempts = 0;
            hoja_deinit(hoja_shutdown);
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
        verified = crc8_verify((uint8_t *)&(data[2]), sizeof(i2cinput_status_s), crc);

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
                connection_attempts = 0;
                rgb_init(global_loaded_settings.rgb_mode, BRIGHTNESS_RELOAD);
                rgb_set_player(_current_connected);
            }
            else
            {
                rgb_set_player(0);
                rgb_flash(_mode_color, -1);
            }
        }
    }
    break;

    case I2C_STATUS_POWER_CODE:
    {
        uint8_t power_code = status.data[0];

        if (power_code == 0)
        {
            hoja_deinit(hoja_shutdown);
        }
        else if (power_code == 1)
        {

            hoja_reboot_memory_u mem = {
                .reboot_reason = ADAPTER_REBOOT_REASON_MODECHANGE,
                .gamepad_mode = INPUT_MODE_SWPRO,
                .gamepad_protocol = INPUT_METHOD_BLUETOOTH};
            reboot_with_memory(mem.value);
        }
    }
    break;

    case I2C_STATUS_HAPTIC_SWITCH:
    {
        haptics_rumble_translate(&(status.data[0]));
    }
    break;

    case I2C_STATUS_HAPTIC_STANDARD:
    {
        hoja_rumble_msg_s rumble_msg_left = {0};
        hoja_rumble_msg_s rumble_msg_right = {0};

        uint8_t l = status.data[0];
        uint8_t r = status.data[1];

        float la = (float) l / 255.0f;
        float ra = (float) r / 255.0f;

        haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, la);
    }
    break;

    case I2C_STATUS_MAC_UPDATE:
    {
        // Paired to Nintendo Switch
        printf("New BT Switch Pairing Completed.");
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

#endif
}

void btinput_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
#if (HOJA_CAPABILITY_BLUETOOTH == 1)

    static i2cinput_input_s input_data = {0};
    static interval_s interval = {0};
    static imu_data_s *imu_tmp = NULL;

    static bool flip = true;
    static bool read_write = true;

    if (interval_run(timestamp, 1000, &interval))
    {
        memset(data_out, 0x80, 24); // Reset buffer to 0x80 for blank

        if (read_write)
        {

            data_out[0] = I2C_CMD_STANDARD;
            data_out[1] = 0;                  // Input CRC location
            data_out[2] = _current_i2c_packet_number; // Response packet number counter

            input_data.buttons_all = buttons->buttons_all;
            input_data.buttons_system = buttons->buttons_system;

            input_data.lx = (uint16_t)analog->lx;
            input_data.ly = (uint16_t)analog->ly;
            input_data.rx = (uint16_t)analog->rx;
            input_data.ry = (uint16_t)analog->ry;

            input_data.lt = (uint16_t)buttons->zl_analog;
            input_data.rt = (uint16_t)buttons->zr_analog;

            imu_tmp = imu_fifo_last();

            if (imu_tmp != NULL)
            {
                input_data.gx = imu_tmp->gx;
                input_data.gy = imu_tmp->gy;
                input_data.gz = imu_tmp->gz;
                input_data.ax = imu_tmp->ax;
                input_data.ay = imu_tmp->ay;
                input_data.az = imu_tmp->az;
            }

            uint8_t crc = crc8_compute((uint8_t *)&input_data, sizeof(i2cinput_input_s));
            data_out[1] = crc;

            memcpy(&(data_out[3]), &input_data, sizeof(i2cinput_input_s));

            int write = i2c_safe_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 16000);
            if (write == HOJA_I2C_MSG_SIZE_OUT)
            {
                analog_send_reset();
                _bt_clear_out();
            }
            
        }
        else
        {
            int read = i2c_safe_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 8000);
            
            if (read == HOJA_I2C_MSG_SIZE_IN)
            {
                _btinput_message_parse(data_in);
                _bt_clear_in();
            }
        }
        read_write = !read_write;
    }
#endif
}