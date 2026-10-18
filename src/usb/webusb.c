#include "usb/webusb.h"
#include "utilities/interval.h"
#include "utilities/tasks.h"
#include "input_shared_types.h"

#include "utilities/settings.h"
#include "utilities/static_config.h"

#include "input/analog.h"
#include "input/imu.h"
#include "input/hover.h"
#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "hoja.h"

#include "input/mapper.h"

#include "hhl_tusb.h"

#include "pico.h"
#include "hardware/sync.h"

static bool _webusb_usb_send_input(const uint8_t *data, uint16_t size)
{
    if (!hhl_tusb_webusb_report_send(data, size))
        return false;
    tasks_mark_sent();
    return true;
}

static const webusb_sink_s _webusb_usb_sink = {
    .send       = hhl_tusb_webusb_report_send,
    .send_input = _webusb_usb_send_input,
    .ready      = hhl_tusb_webusb_report_ready_blocking,
};

static const webusb_sink_s *_webusb_sink = &_webusb_usb_sink;

// The dongle's and BLE's sinks run the radio stack, which belongs to the core that set them. A
// packet from the other core (the stick processing's snapback capture) waits here, and that core's
// task loop sends it (webusb_send_rawinput).
#define WEBUSB_DEFERRED_LEN 4
static uint8_t _webusb_deferred[WEBUSB_DEFERRED_LEN][64];
static uint8_t _webusb_deferred_size[WEBUSB_DEFERRED_LEN];
static volatile uint8_t _webusb_deferred_head = 0;
static volatile uint8_t _webusb_deferred_tail = 0;
static uint _webusb_sink_core = 0;

void webusb_set_sink(const webusb_sink_s *sink)
{
    // A session ended or began elsewhere: the game gets its input back
    mapper_hold_game_inputs(false);
    _webusb_sink = sink ? sink : &_webusb_usb_sink;
    _webusb_sink_core = get_core_num();
    _webusb_deferred_tail = _webusb_deferred_head;
}

const webusb_sink_s *webusb_get_sink(void)
{
    return (_webusb_sink == &_webusb_usb_sink) ? NULL : _webusb_sink;
}

uint8_t _webusb_focused_hover = 0;
uint8_t _webusb_report_mode = WEBUSB_INPUT_RAW;

bool _ready_to_go = false;

static void _webusb_mark_unready(void)
{
    if(!_ready_to_go)
        return;

    _ready_to_go = false;
    mapper_webusb_remap_preview_end();
}

bool webusb_outputting_check()
{
    return _ready_to_go;
}

// Can be used to block until webUSB
// is clear and ready to send output data
// Set timeout to value greater than zero.
bool webusb_ready_blocking(int timeout)
{
    return _webusb_sink->ready(timeout);
}


static void _webusb_defer(const uint8_t *data, uint16_t size)
{
    uint32_t irq = save_and_disable_interrupts();
    uint8_t next = (uint8_t)((_webusb_deferred_head + 1u) % WEBUSB_DEFERRED_LEN);
    if (next != _webusb_deferred_tail && size <= 64)
    {
        memcpy(_webusb_deferred[_webusb_deferred_head], data, size);
        _webusb_deferred_size[_webusb_deferred_head] = (uint8_t)size;
        __dmb();
        _webusb_deferred_head = next;
    }
    restore_interrupts(irq);
}

static void _webusb_send_deferred(void)
{
    while (_webusb_deferred_tail != _webusb_deferred_head)
    {
        uint8_t i = _webusb_deferred_tail;
        if (!_webusb_sink->send(_webusb_deferred[i], _webusb_deferred_size[i]))
            _webusb_mark_unready();
        _webusb_deferred_tail = (uint8_t)((i + 1u) % WEBUSB_DEFERRED_LEN);
    }
}

void webusb_send_bulk(const uint8_t *data, uint16_t size)
{
    if (!_ready_to_go)
    {
        return;
    }

    if ((_webusb_sink != &_webusb_usb_sink) && (get_core_num() != _webusb_sink_core))
    {
        _webusb_defer(data, size);
        return;
    }

    if (!_webusb_sink->send(data, size))
    {
        _webusb_mark_unready();
    }
}

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
void webusb_send_rawinput(uint64_t timestamp)
{
    static interval_s interval_ready = {0};
    static interval_s interval = {0};
    static imu_data_s imu = {0};

    static bool ready = false;

    if(!_ready_to_go) return;

    _webusb_send_deferred();

    if(!ready)
    {
        if(webusb_ready_blocking(256))
        {
            ready = true;
        }
        else _webusb_mark_unready();
    }

    if (interval_run(timestamp, 8000, &interval) && ready)
    {
        static uint8_t webusb_input_report[64] = {0};

        // Charge status
        static battery_status_s batstat = {0};
        static fuelgauge_status_s fgstat = {0};

        battery_get_status(&batstat);
        fuelgauge_get_status(&fgstat);

        webusb_input_report[1] = (uint8_t)batstat.charging | ((uint8_t)batstat.charging_done << 1);
        webusb_input_report[2] = fgstat.percent;

        static imu_data_s imu = {0};
        imu_access_safe(&imu);
        memcpy(&webusb_input_report[3], &imu.ax, 2);
        memcpy(&webusb_input_report[5], &imu.ay, 2);
        memcpy(&webusb_input_report[7], &imu.az, 2);

        memcpy(&webusb_input_report[9], &imu.gx, 2);
        memcpy(&webusb_input_report[11], &imu.gy, 2);
        memcpy(&webusb_input_report[13], &imu.gz, 2);

        analog_data_s joysticks;
        switch(_webusb_report_mode)
        {
            default:
            case WEBUSB_INPUT_RAW:
                webusb_input_report[0] = WEBUSB_INPUT_RAW;

                mapper_input_s mapper = mapper_get_translated_input();
                analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);

                // One value is focused and we get the full uint16_t value
                uint16_t focused_val = mapper.inputs[_webusb_focused_hover] | (mapper.presses[_webusb_focused_hover] ? 0x8000 : 0);
                webusb_input_report[15] = (focused_val & 0xFF00) >> 8;
                webusb_input_report[16] = (focused_val & 0xFF); 

                // Remainder of values are downsampled to 8 bits
                for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
                {
                    uint16_t truncated = mapper.inputs[i] > 4095 ? 4095 : mapper.inputs[i];
                    uint8_t press_state = (mapper.presses[i] ? 0x80 : 0);
                    uint8_t scaled = ((truncated>> 5) & 0x7F) | press_state;
                    webusb_input_report[17+i] = scaled;
                }
            break;

            case WEBUSB_INPUT_JOYSTICKS:
                webusb_input_report[0] = WEBUSB_INPUT_JOYSTICKS;

                analog_access_safe(&joysticks, ANALOG_ACCESS_SNAPBACK_DATA);
                uint16_t lx = (uint16_t) (joysticks.lx + 2048);
                uint16_t ly = (uint16_t) (joysticks.ly + 2048);
                uint16_t rx = (uint16_t) (joysticks.rx + 2048);
                uint16_t ry = (uint16_t) (joysticks.ry + 2048);
                webusb_input_report[15] = (lx & 0xFF00) >> 8;
                webusb_input_report[16] = (lx & 0xFF);       
                webusb_input_report[17] = (ly & 0xFF00) >> 8;
                webusb_input_report[18] = (ly & 0xFF);       
                webusb_input_report[19] = (rx & 0xFF00) >> 8;
                webusb_input_report[20] = (rx & 0xFF);       
                webusb_input_report[21] = (ry & 0xFF00) >> 8;
                webusb_input_report[22] = (ry & 0xFF);

                analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);
                lx = (uint16_t) (joysticks.lx + 2048);
                ly = (uint16_t) (joysticks.ly + 2048);
                rx = (uint16_t) (joysticks.rx + 2048);
                ry = (uint16_t) (joysticks.ry + 2048);
                webusb_input_report[23] = (lx & 0xFF00) >> 8;
                webusb_input_report[24] = (lx & 0xFF);
                webusb_input_report[25] = (ly & 0xFF00) >> 8;
                webusb_input_report[26] = (ly & 0xFF);
                webusb_input_report[27] = (rx & 0xFF00) >> 8;
                webusb_input_report[28] = (rx & 0xFF);
                webusb_input_report[29] = (ry & 0xFF00) >> 8;
                webusb_input_report[30] = (ry & 0xFF);
            break;
        }

        if (!_webusb_sink->send_input(webusb_input_report, 64))
        {
            _webusb_mark_unready();
        }

        ready = false;
    }
}

void webusb_command_confirm_cb(cfg_block_t config_block, uint8_t cmd, bool success, uint8_t *data, uint32_t size)
{
    uint8_t cmd_confirm_buffer[64] = {0x00};
    cmd_confirm_buffer[0] = WEBUSB_ID_CONFIG_COMMAND;
    cmd_confirm_buffer[1] = config_block;
    cmd_confirm_buffer[2] = cmd;
    cmd_confirm_buffer[3] = success;

    if(data) 
        memcpy(&cmd_confirm_buffer[4], data, size);

    webusb_send_bulk(cmd_confirm_buffer, 4+size);
}

static uint32_t _webusb_crc32(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

// The app keeps the blocks it read before and reads again only those whose CRC32 changed. Two
// packets: [id][0 config blocks][count][CRC32 LE...] and [id][1 static blocks][count][CRC32 LE...],
// each block's CRC over exactly the bytes a read of it sends (static blocks refreshed first).
static void _webusb_send_fingerprints(void)
{
    uint8_t out[64];
    for (uint8_t kind = 0; kind < 2; kind++)
    {
        memset(out, 0, sizeof(out));
        uint8_t count = (kind == 0) ? (uint8_t)CFG_BLOCK_MAX : (uint8_t)STATIC_BLOCK_MAX;
        out[0] = WEBUSB_ID_BLOCK_FINGERPRINTS;
        out[1] = kind;
        out[2] = count;
        for (uint8_t i = 0; i < count && (3u + 4u * (i + 1u)) <= sizeof(out); i++)
        {
            uint8_t *bytes = NULL;
            uint32_t len = 0;
            bool known = (kind == 0) ? settings_config_block_bytes((cfg_block_t)i, &bytes, &len)
                                     : static_config_block_bytes((static_block_t)i, &bytes, &len);
            uint32_t crc = known ? _webusb_crc32(bytes, len) : 0;
            memcpy(&out[3u + 4u * i], &crc, 4);
        }
        webusb_send_bulk(out, sizeof(out));
    }
}

void webusb_command_handler(uint8_t *data, uint32_t size)
{
    // The app's motion view wants standard IMU data. Through the dongle or BLE a game may be using
    // the IMU in another mode (a Switch's quaternions), so leave it be.
    if (!_ready_to_go && (_webusb_sink == &_webusb_usb_sink))
    {
        imu_set_read_mode(IMU_MODE_STANDARD);
    }

    _ready_to_go = true;
    mapper_app_heard();
    switch(data[0])
    {
        case WEBUSB_ID_READ_CONFIG_BLOCK:
            settings_return_config_block(data[1], webusb_send_bulk);
        break;

        case WEBUSB_ID_WRITE_CONFIG_BLOCK:
            settings_write_config_block(data[1], data);
        break;

        case WEBUSB_ID_READ_STATIC_BLOCK:
            static_config_read_block(data[1], webusb_send_bulk);
        break;

        case WEBUSB_ID_CONFIG_COMMAND:
            settings_config_command(data[1], data[2], size > 3 ? data[3] : 0xFF);
        break;

        case WEBUSB_ID_BLOCK_FINGERPRINTS:
            _webusb_send_fingerprints();
        break;

        case WEBUSB_ID_INPUT_COMMAND:
            switch(data[1])
            {
                case 0:
                // Change reporting mode
                _webusb_report_mode = data[2];
                break;

                case 1:
                // Change focused input ID
                _webusb_focused_hover = data[2];
                break;
            }
        break;

        case WEBUSB_LEGACY_SET_BOOTLOADER:
            // Do nothing
        break;

        case WEBUSB_LEGACY_GET_FW_VERSION:
            // Do nothing
        break;
    }   
}

void webusb_version_read(uint8_t type)
{
    (void) type;
}

// Read out all configuration blocks
void webusb_settings_read()
{
    // Read all config blocks
    for(int i = 0; i < CFG_BLOCK_MAX; i++)
    {
        settings_return_config_block(i, webusb_send_bulk);
    }
}
