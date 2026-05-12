#include <string.h>
#include <ns_lib.h>

#include "cores/core_switch.h"

#include "transport/transport.h"
#include "hoja_shared_types.h"
#include "input/imu.h"
#include "input/mapper.h"
#include "input/dpad.h"

#include "utilities/settings.h"
#include "utilities/pcm.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "devices/haptics.h"

#include "hal/sys_hal.h"

#include "hoja_usb.h"
#include "hoja.h"

#include "usb/webusb.h"

#include "devices_shared_types.h"

#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define SWITCH_HID_NAME "Pro Controller"
static const uint16_t _switch_hid_pid = 0x2009;
static const uint16_t _switch_hid_vid = 0x057E;

static const hoja_usb_device_descriptor_t _swpro_device_descriptor = {
    .bLength = sizeof(hoja_usb_device_descriptor_t),
    .bDescriptorType = HUSB_DESC_DEVICE,
    .bcdUSB = 0x0210,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = _switch_hid_vid,
    .idProduct = _switch_hid_pid,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

#define SWPRO_CONFIG_DESCRIPTOR_LEN 64

/* Composite FS configuration: HID + vendor/WebUSB (length patched for HID report descriptor). */
static const uint8_t _swpro_usb_config_template[SWPRO_CONFIG_DESCRIPTOR_LEN] = {
    HUSB_CONFIG_DESCRIPTOR(1, 2, 0, SWPRO_CONFIG_DESCRIPTOR_LEN, HUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),

    9,
    HUSB_DESC_INTERFACE,
    0x00,
    0x00,
    0x02,
    HUSB_CLASS_HID,
    0x00,
    0x00,
    0x00,
    9,
    HHID_DESC_TYPE_HID,
    HUSB_U16_TO_U8S_LE(0x0111),
    0,
    1,
    HHID_DESC_TYPE_REPORT,
    HUSB_U16_TO_U8S_LE(NS_HID_USB_REPORT_DESCRIPTOR_LEN),
    7,
    HUSB_DESC_ENDPOINT,
    0x81,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    1,
    7,
    HUSB_DESC_ENDPOINT,
    0x01,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    1,

    9,
    HUSB_DESC_INTERFACE,
    0x01,
    0x00,
    0x02,
    HUSB_CLASS_VENDOR_SPECIFIC,
    0x00,
    0x00,
    0x00,
    7,
    HUSB_DESC_ENDPOINT,
    0x82,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
    7,
    HUSB_DESC_ENDPOINT,
    0x02,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
};

static uint8_t _swpro_usb_config_runtime[SWPRO_CONFIG_DESCRIPTOR_LEN];

static core_hid_device_t _switch_hid_device_usb = {0};
static core_hid_device_t _switch_hid_device_bt = {0};

static core_params_s *_core_switch_params = NULL;
static uint8_t _switch_report_size = 64;

#define HAPTICS_FP_SHIFT (1u << 12u)
static uint16_t _haptic_fp_hi[128];
static uint16_t _haptic_fp_lo[128];
static uint16_t _haptic_fp_amp[256];
static bool _haptic_fp_ready = false;

/* Same mapping as legacy switch_haptics `_init_amp_idx_lookup` (wire index -> amplitude LUT row). */
static uint8_t _haptic_amp_row[128];

static void _core_switch_init_amp_row_table(void)
{
#define AMP_RANGE_START (-8.0f)
#define AMP_INTERVAL 0.03125f
#define AMP_STARTING_FLOAT (-7.9375f)
    for (int i = 0; i < 128; ++i)
    {
        float tmp = 0.0f;

        if (i == 0)
        {
            tmp = -8.0f;
        }
        else if (i < 16)
        {
            tmp = 0.25f * (float)i - 7.75f;
        }
        else if (i < 32)
        {
            tmp = 0.0625f * (float)i - 4.9375f;
        }
        else
        {
            tmp = 0.03125f * (float)i - 3.96875f;
        }

        float fidx = (tmp / AMP_INTERVAL);
        int16_t idx = 255 + (int16_t)fidx;

        if (!i)
        {
            idx = 0;
        }
        if (idx > 255)
        {
            idx = 255;
        }
        if (idx < 0)
        {
            idx = 0;
        }

        _haptic_amp_row[i] = (uint8_t)idx;
    }
    (void)AMP_RANGE_START;
    (void)AMP_STARTING_FLOAT;
#undef AMP_RANGE_START
#undef AMP_INTERVAL
#undef AMP_STARTING_FLOAT
}

static void _core_switch_init_haptic_fp_tables(void)
{
    if (_haptic_fp_ready)
    {
        return;
    }
    ns_api_generate_fp_haptic_frequency_tables(HAPTICS_FP_SHIFT, PCM_SINE_TABLE_SIZE, PCM_SAMPLE_RATE, _haptic_fp_hi,
                                               _haptic_fp_lo);
    ns_api_generate_fp_amplitude_multiplier_table(HAPTICS_FP_SHIFT, _haptic_fp_amp);
    _haptic_fp_ready = true;
}

static void _core_switch_fill_ns_colors(ns_colordata_s *c)
{
    c->body.r = (uint8_t)((gamepad_config->gamepad_color_body >> 16) & 0xFF);
    c->body.g = (uint8_t)((gamepad_config->gamepad_color_body >> 8) & 0xFF);
    c->body.b = (uint8_t)(gamepad_config->gamepad_color_body & 0xFF);
    c->body.reserved = 0;

    c->l_grip.r = (uint8_t)((gamepad_config->gamepad_color_grip_left >> 16) & 0xFF);
    c->l_grip.g = (uint8_t)((gamepad_config->gamepad_color_grip_left >> 8) & 0xFF);
    c->l_grip.b = (uint8_t)(gamepad_config->gamepad_color_grip_left & 0xFF);
    c->l_grip.reserved = 0;

    c->r_grip.r = (uint8_t)((gamepad_config->gamepad_color_grip_right >> 16) & 0xFF);
    c->r_grip.g = (uint8_t)((gamepad_config->gamepad_color_grip_right >> 8) & 0xFF);
    c->r_grip.b = (uint8_t)(gamepad_config->gamepad_color_grip_right & 0xFF);
    c->r_grip.reserved = 0;

    c->buttons.r = (uint8_t)((gamepad_config->gamepad_color_buttons >> 16) & 0xFF);
    c->buttons.g = (uint8_t)((gamepad_config->gamepad_color_buttons >> 8) & 0xFF);
    c->buttons.b = (uint8_t)(gamepad_config->gamepad_color_buttons & 0xFF);
    c->buttons.reserved = 0;
}

static ns_transport_t _core_switch_transport_from_params(const core_params_s *params)
{
    switch (params->transport_type)
    {
    case GAMEPAD_TRANSPORT_USB:
        return NS_TRANSPORT_USB;
    case GAMEPAD_TRANSPORT_BLUETOOTH:
        return NS_TRANSPORT_BTC;
    default:
        return NS_TRANSPORT_UNDEFINED;
    }
}

static void _core_switch_prepare_device_mac(uint8_t out[6], const core_params_s *params)
{
    memcpy(out, gamepad_config->gamepad_mac_address, 6);
    out[5] += (uint8_t)params->core_report_format;
    if (params->transport_type == GAMEPAD_TRANSPORT_USB)
    {
        out[4] += 1;
    }
}

static bool _core_switch_apply_ns_hid_descriptors(ns_transport_t tr)
{
    memcpy(_swpro_usb_config_runtime, _swpro_usb_config_template, sizeof(_swpro_usb_config_runtime));
    _swpro_usb_config_runtime[25] = (uint8_t)(NS_HID_USB_REPORT_DESCRIPTOR_LEN & 0xFFu);
    _swpro_usb_config_runtime[26] = (uint8_t)((NS_HID_USB_REPORT_DESCRIPTOR_LEN >> 8) & 0xFFu);

    const uint8_t *hid = NULL;
    uint16_t hid_len = 0;
    const uint8_t *cfg = NULL;
    uint16_t cfg_len = 0;
    uint16_t vid = 0;
    uint16_t pid = 0;

    if (!ns_hid_get_descriptor_params(&hid, &hid_len, &cfg, &cfg_len, &vid, &pid) || hid == NULL || hid_len == 0u)
    {
        return false;
    }

    if (tr == NS_TRANSPORT_USB)
    {
        _swpro_hid_usb_ptr = hid;
        _swpro_hid_usb_len = hid_len;

        _switch_hid_device_usb.config_descriptor = _swpro_usb_config_runtime;
        _switch_hid_device_usb.config_descriptor_len = SWPRO_CONFIG_DESCRIPTOR_LEN;
        _switch_hid_device_usb.hid_report_descriptor = hid;
        _switch_hid_device_usb.hid_report_descriptor_len = hid_len;
        _switch_hid_device_usb.device_descriptor = &_swpro_device_descriptor;
        _switch_hid_device_usb.vid = _swpro_device_descriptor.idVendor;
        _switch_hid_device_usb.pid = _swpro_device_descriptor.idProduct;
        strncpy(_switch_hid_device_usb.name, ns_hid_get_device_name(), sizeof(_switch_hid_device_usb.name) - 1);
        _switch_hid_device_usb.name[sizeof(_switch_hid_device_usb.name) - 1] = '\0';
    }
    else if (tr == NS_TRANSPORT_BTC)
    {
        _swpro_hid_bt_ptr = hid;
        _swpro_hid_bt_len = hid_len;

        _switch_hid_device_bt.config_descriptor = _swpro_usb_config_runtime;
        _switch_hid_device_bt.config_descriptor_len = SWPRO_CONFIG_DESCRIPTOR_LEN;
        _switch_hid_device_bt.hid_report_descriptor = hid;
        _switch_hid_device_bt.hid_report_descriptor_len = hid_len;
        _switch_hid_device_bt.device_descriptor = &_swpro_device_descriptor;
        _switch_hid_device_bt.vid = _swpro_device_descriptor.idVendor;
        _switch_hid_device_bt.pid = _swpro_device_descriptor.idProduct;
        strncpy(_switch_hid_device_bt.name, ns_hid_get_device_name(), sizeof(_switch_hid_device_bt.name) - 1);
        _switch_hid_device_bt.name[sizeof(_switch_hid_device_bt.name) - 1] = '\0';
    }

    (void)vid;
    (void)pid;
    (void)cfg;
    (void)cfg_len;
    return true;
}

static uint8_t _core_switch_bt_out_total_len(uint8_t report_id)
{
    switch (report_id)
    {
    case 0x30:
    case 0x21:
    case 0x81:
        return 49u;
    default:
        return 64u;
    }
}

void _core_switch_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0u)
    {
        return;
    }
    ns_api_output_tunnel(data, len);
}

bool _core_switch_get_generated_report(core_report_s *out)
{
    uint8_t buf[64] = {0};

    if (!ns_api_generate_inputreport(buf))
    {
        return false;
    }

    out->reportformat = CORE_REPORTFORMAT_SWPRO;
    memcpy(out->data, buf, sizeof(buf));

    if (_core_switch_params && _core_switch_params->transport_type == GAMEPAD_TRANSPORT_BLUETOOTH)
    {
        out->size = _core_switch_bt_out_total_len(buf[0]);
    }
    else
    {
        out->size = 64;
    }

    return true;
}

void ns_api_hook_get_time_ms(uint64_t *ms)
{
    if (!ms)
    {
        return;
    }
    sys_hal_time_ms(ms);
}

uint8_t ns_api_hook_get_random_u8(void)
{
    return (uint8_t)(sys_hal_random() & 0xFF);
}

void ns_api_hook_get_input(ns_input_s *out)
{
    if (!out)
    {
        return;
    }

    mapper_input_s input = mapper_get_input();

    bool dpad[4] = {input.presses[SWITCH_CODE_DOWN], input.presses[SWITCH_CODE_RIGHT], input.presses[SWITCH_CODE_LEFT],
                    input.presses[SWITCH_CODE_UP]};
    dpad_translate_input(dpad);

    memset(out, 0, sizeof(*out));
    out->down = dpad[0];
    out->right = dpad[1];
    out->left = dpad[2];
    out->up = dpad[3];

    out->y = input.presses[SWITCH_CODE_Y];
    out->x = input.presses[SWITCH_CODE_X];
    out->a = input.presses[SWITCH_CODE_A];
    out->b = input.presses[SWITCH_CODE_B];

    out->minus = input.presses[SWITCH_CODE_MINUS];
    out->plus = input.presses[SWITCH_CODE_PLUS];
    out->home = input.presses[SWITCH_CODE_HOME];
    out->capture = input.presses[SWITCH_CODE_CAPTURE];

    out->r_js = input.presses[SWITCH_CODE_RS];
    out->l_js = input.presses[SWITCH_CODE_LS];

    out->r = input.presses[SWITCH_CODE_R];
    out->l = input.presses[SWITCH_CODE_L];
    out->zl = input.presses[SWITCH_CODE_ZL];
    out->zr = input.presses[SWITCH_CODE_ZR];

    uint16_t lx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LX_LEFT], input.inputs[SWITCH_CODE_LX_RIGHT]);
    uint16_t ly = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LY_DOWN], input.inputs[SWITCH_CODE_LY_UP]);
    uint16_t rx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RX_LEFT], input.inputs[SWITCH_CODE_RX_RIGHT]);
    uint16_t ry = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RY_DOWN], input.inputs[SWITCH_CODE_RY_UP]);

    out->ls_x = (uint16_t)CORE_SWITCH_CLAMP(lx, 0, 4095);
    out->ls_y = (uint16_t)CORE_SWITCH_CLAMP(ly, 0, 4095);
    out->rs_x = (uint16_t)CORE_SWITCH_CLAMP(rx, 0, 4095);
    out->rs_y = (uint16_t)CORE_SWITCH_CLAMP(ry, 0, 4095);
}

void ns_api_hook_get_powerstatus(ns_powerstatus_s *out)
{
    if (!out)
    {
        return;
    }

    fuelgauge_status_s fstat = {0};
    fuelgauge_get_status(&fstat);

    battery_status_s bstat = {0};
    battery_get_status(&bstat);

    bat_status_u s = {.bat_lvl = 4, .charging = 1, .connection = 0};

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

    out->val = s.val;
}

void ns_api_hook_set_led(int player_leds)
{
    if (player_leds < 0)
    {
        return;
    }
    tp_evt_s pevt = {.evt = TP_EVT_PLAYERLED, .evt_playernumber = {.player_number = (uint8_t)player_leds}};
    transport_evt_cb(pevt);
}

void ns_api_hook_set_power(uint8_t shutdown)
{
    if (!shutdown)
    {
        return;
    }
    tp_evt_s pevt = {.evt = TP_EVT_POWERCOMMAND, .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN}};
    transport_evt_cb(pevt);
}

void ns_api_hook_set_usbpair(ns_usbpair_s pairing_data)
{
    memcpy(gamepad_config->host_mac_switch, pairing_data.host_mac, 6);
    memcpy(gamepad_config->link_key_switch, pairing_data.link_key, 16);
    settings_commit_blocks();
}

void ns_api_hook_set_imu_mode(ns_imu_mode_t imu_mode)
{
    (void)imu_mode;
}

void ns_api_hook_get_imu(ns_gyrodata_s *out)
{
    if (!out)
    {
        return;
    }

    imu_data_s imu = {0};
    imu_access_safe(&imu);

    out->ax = imu.ax;
    out->ay = imu.ay;
    out->az = imu.az;
    out->gx = imu.gx;
    out->gy = imu.gy;
    out->gz = imu.gz;
    out->timestamp_us = imu.timestamp;
}

void ns_api_hook_get_quaternion(ns_quaternion_s *out)
{
    if (!out)
    {
        return;
    }

    quaternion_s q = {0};
    imu_quaternion_access_safe(&q);

    out->raw[0] = q.x;
    out->raw[1] = q.y;
    out->raw[2] = q.z;
    out->raw[3] = q.w;
    out->ax = q.ax;
    out->ay = q.ay;
    out->az = q.az;
    out->timestamp_us = q.timestamp_ms * 1000ULL;
}

void ns_api_hook_set_haptic_packet_raw(ns_haptics_packet_raw_s *packet)
{
    if (!packet || webusb_outputting_check())
    {
        return;
    }

    if (!_haptic_fp_ready)
    {
        _core_switch_init_haptic_fp_tables();
    }

    haptic_packet_s hp = {0};
    uint8_t n = packet->sample_count;
    if (n > 3u)
    {
        n = 3u;
    }
    hp.count = n;

    if (n == 0u)
    {
        hp.count = 1u;
        hp.pairs[0].hi_amplitude_fixed = 0;
        hp.pairs[0].lo_amplitude_fixed = 0;
        hp.pairs[0].hi_frequency_increment = 0;
        hp.pairs[0].lo_frequency_increment = 0;
        haptics_set_hd(&hp);
        return;
    }

    for (uint8_t i = 0; i < n; i++)
    {
        uint8_t hai = packet->samples[i].hi_amplitude_idx;
        uint8_t lai = packet->samples[i].lo_amplitude_idx;
        uint8_t hfi = packet->samples[i].hi_frequency_idx;
        uint8_t lfi = packet->samples[i].lo_frequency_idx;
        if (hai >= NS_LIB_HAPTICS_AMP_LUT_LEN)
        {
            hai = (uint8_t)(NS_LIB_HAPTICS_AMP_LUT_LEN - 1u);
        }
        if (lai >= NS_LIB_HAPTICS_AMP_LUT_LEN)
        {
            lai = (uint8_t)(NS_LIB_HAPTICS_AMP_LUT_LEN - 1u);
        }
        if (hfi >= NS_LIB_HAPTICS_FREQ_LUT_LEN)
        {
            hfi = (uint8_t)(NS_LIB_HAPTICS_FREQ_LUT_LEN - 1u);
        }
        if (lfi >= NS_LIB_HAPTICS_FREQ_LUT_LEN)
        {
            lfi = (uint8_t)(NS_LIB_HAPTICS_FREQ_LUT_LEN - 1u);
        }

        hp.pairs[i].hi_amplitude_fixed = _haptic_fp_amp[hai];
        hp.pairs[i].lo_amplitude_fixed = _haptic_fp_amp[lai];
        hp.pairs[i].hi_frequency_increment = _haptic_fp_hi[hfi];
        hp.pairs[i].lo_frequency_increment = _haptic_fp_lo[lfi];
    }

    haptics_set_hd(&hp);
}

void core_switch_ns_feed_hd_rumble_wire4(const uint8_t *data)
{
    if (!data)
    {
        return;
    }
    ns_haptics_rumble_translate(data);
}

void core_switch_ns_output_tunnel(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0u)
    {
        return;
    }
    ns_api_output_tunnel(data, len);
}

uint8_t *core_switch_ns_analog_calibration_blob(void)
{
    return ns_analog_calibration_data();
}

void core_switch_ns_analog_calibration_reset_defaults(void)
{
    ns_analog_calibration_init();
}

void core_switch_ns_motion_quat_step(void)
{
    /* Fusion is owned by imu.c; NS reports read imu_quaternion_access_safe in the hook. */
}

static void _core_switch_arbitrary_playback_internal(uint8_t intensity)
{
    uint8_t scale = (uint8_t)(intensity >> 3u);
    uint8_t translated_intensity = scale;
    uint8_t translated_intensity_lo = (uint8_t)(scale + 2u);
    if (translated_intensity_lo > 127u)
    {
        translated_intensity_lo = 127u;
    }

    uint8_t freq_offset = 0;
    uint8_t target_freq = 64;

    if (translated_intensity == 36u)
    {
        freq_offset = 0;
    }
    else if (translated_intensity < 36u)
    {
        uint8_t remainder = (uint8_t)(36u - translated_intensity);
        freq_offset = remainder > 36u ? 36u : remainder;
        target_freq -= freq_offset;
    }
    else
    {
        uint8_t remainder = (uint8_t)(translated_intensity - 36u);
        freq_offset = remainder > 36u ? 36u : remainder;
        target_freq += freq_offset;
    }

    if (!_haptic_fp_ready)
    {
        _core_switch_init_haptic_fp_tables();
    }

    if (!intensity)
    {
        haptic_packet_s packet = {0};
        packet.count = 1;
        packet.pairs[0].hi_amplitude_fixed = 0;
        packet.pairs[0].lo_amplitude_fixed = 0;
        packet.pairs[0].hi_frequency_increment = 0;
        packet.pairs[0].lo_frequency_increment = 0;
        haptics_set_hd(&packet);
        return;
    }

    uint8_t dbg_ahi = _haptic_amp_row[translated_intensity > 127u ? 127u : translated_intensity];
    uint8_t dbg_alo = _haptic_amp_row[translated_intensity_lo > 127u ? 127u : translated_intensity_lo];
    uint8_t dbg_hi = target_freq > 127u ? 127u : target_freq;
    uint8_t dbg_lo = target_freq > 127u ? 127u : target_freq;

    haptic_packet_s packet = {0};
    packet.count = 1;
    packet.pairs[0].hi_amplitude_fixed = _haptic_fp_amp[dbg_ahi];
    packet.pairs[0].lo_amplitude_fixed = _haptic_fp_amp[dbg_alo];
    packet.pairs[0].hi_frequency_increment = _haptic_fp_hi[dbg_hi];
    packet.pairs[0].lo_frequency_increment = _haptic_fp_lo[dbg_lo];

    haptics_set_hd(&packet);
}

void core_switch_ns_arbitrary_playback(uint8_t intensity)
{
    _core_switch_arbitrary_playback_internal(intensity);
}

bool core_switch_init(core_params_s *params)
{
    _core_switch_params = params;

    ns_device_config_s cfg = {0};
    cfg.type = NS_DEVTYPE_PROCON;
    cfg.transport = _core_switch_transport_from_params(params);
    if (cfg.transport == NS_TRANSPORT_UNDEFINED)
    {
        return false;
    }

    _core_switch_fill_ns_colors(&cfg.colors);
    cfg.gyro_full_scale_dps = 2000.0f;
    cfg.gyro_rad_per_lsb = 0.0f;

    _core_switch_prepare_device_mac(cfg.device_mac, params);
    memcpy(cfg.host_mac, gamepad_config->host_mac_switch, 6);

    if (ns_api_init(&cfg) != NS_CONFIG_OK)
    {
        return false;
    }

    _core_switch_init_amp_row_table();
    _core_switch_init_haptic_fp_tables();

    if (!_core_switch_apply_ns_hid_descriptors(cfg.transport))
    {
        return false;
    }

    switch (params->transport_type)
    {
    case GAMEPAD_TRANSPORT_USB:
        _switch_report_size = 64;
        params->hid_device = &_switch_hid_device_usb;
        params->core_pollrate_us = 8000;
        break;

    case GAMEPAD_TRANSPORT_BLUETOOTH:
        _switch_report_size = 49;
        params->hid_device = &_switch_hid_device_bt;
        params->core_pollrate_us = 8000;
        break;

    default:
        return false;
    }

    params->sys_gyro_task = imu_forced_task;

    params->core_report_format = CORE_REPORTFORMAT_SWPRO;
    params->core_report_generator = _core_switch_get_generated_report;
    params->core_report_tunnel = _core_switch_report_tunnel_cb;

    return transport_init(params);
}
