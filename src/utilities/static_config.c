#include "utilities/static_config.h"
#include "hoja.h"
#include "board_config.h"
#include "usb/webusb.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "input/imu.h"

// Bluetooth driver nonsense
#include "devices/bluetooth.h"
#include "transport/transport_bt.h"
#include "transport/transport_wlan.h"

#include <string.h>

// Device name/maker and the manifest/firmware/manual URLs now live in the
// board's hoja_config_s (populated in main.c) and are copied into device_static
// at runtime by _device_static_refresh().

#if !defined(HOJA_DEVICE_SNES_SUPPORTED)
 #warning "HOJA_DEVICE_SNES_SUPPORTED undefined. SNES/NES disabled."
 #define SNES_SUPPORT 0
#else 
 #define SNES_SUPPORT HOJA_DEVICE_SNES_SUPPORTED
#endif

#if !defined(HOJA_DEVICE_JOYBUS_SUPPORTED)
 #warning "HOJA_DEVICE_JOYBUS_SUPPORTED undefined. N64/GameCube disabled."
 #define JOYBUS_SUPPORT 0
#else 
 #define JOYBUS_SUPPORT HOJA_DEVICE_JOYBUS_SUPPORTED
#endif

#if !defined(HOJA_DEVICE_FCC_ID_TEXT) 
    #define FCC_ID_TEXT "~"
#else 
    #define FCC_ID_TEXT HOJA_DEVICE_FCC_ID_TEXT
#endif

// name/maker/manifest_url/firmware_url/manual_url are filled at runtime from the
// hoja config (see _device_static_refresh); only the compile-time fields are
// initialized here.
deviceInfoStatic_s    device_static = {
    .fw_version     = FIRMWARE_VERSION_TIMESTAMP,
    .snes_supported = SNES_SUPPORT,
    .joybus_supported = JOYBUS_SUPPORT
};

#define ANALOG_SUPPORTED (ALX | ALY | ARX | ARY | ALT | ART)

#if defined(HOJA_ANALOG_INVERT_ALLOWED)
    #define ANALOG_INVERT_ALLOWED 1
#else 
    #define ANALOG_INVERT_ALLOWED 0
#endif

analogInfoStatic_s analog_static = {
    .axis_lx = 0,
    .axis_ly = 0,
    .axis_rx = 0,
    .axis_ry = 0,
    .axis_lt = 0,
    .axis_rt = 0,
    .invert_allowed = 0,
};

// Populated at runtime from the selected IMU driver's channel count (see
// _imu_static_refresh); a board with no IMU driver reports all axes absent.
imuInfoStatic_s imu_static = {0};

// All battery static fields are populated at runtime in
// _battery_static_refresh(): capacity + battery_part_number from the hoja
// config, pmic/fuelgauge part numbers from their assigned drivers.
batteryInfoStatic_s battery_static = {
    .battery_capacity_mah = 0,
    .battery_part_number  = "N/A",
    .pmic_part_number = "N/A",
    .fuelgauge_part_number = "N/A",
};

#if !defined(HOJA_HAPTICS_DRIVER)
    #warning "HOJA_HAPTICS_DRIVER is not defined in board_config.h. Rumble features will be disabled."
    #define HAPTICS_HD_EN 0
    #define HAPTICS_SD_EN 0
#else 
    #if (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
        #define HAPTICS_HD_EN 1
    #else 
        #define HAPTICS_HD_EN 0
    #endif

    #if (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_ERM_HAL)
        #define HAPTICS_SD_EN 1
    #else
        #define HAPTICS_SD_EN 0
    #endif
#endif

const hapticInfoStatic_s    haptic_static = {
    .haptic_hd = HAPTICS_HD_EN,
    .haptic_sd = HAPTICS_SD_EN,
};

#if !defined(HOJA_INPUT_SLOTS)
    #warning "HOJA_INPUT_SLOTS is not defined. Falling back to default params"
    #define HOJA_INPUT_SLOTS { \
        (inputInfoSlot_s) {/*South*/.input_name="South", .input_type=INPUT_TYPE_DIGITAL, .rgb_group=1}, \
        (inputInfoSlot_s) {/*East*/.input_name="East", .input_type=INPUT_TYPE_DIGITAL, .rgb_group=2}, \
        (inputInfoSlot_s) {/*West*/.input_name="West", .input_type=INPUT_TYPE_DIGITAL, .rgb_group=3}, \
        (inputInfoSlot_s) {/*North*/.input_name="North", .input_type=INPUT_TYPE_DIGITAL, .rgb_group=4}, \
        (inputInfoSlot_s) {/*Up*/.input_name="Up", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Down*/.input_name="Down", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Left*/.input_name="Left", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Right*/.input_name="Right", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*SL*/.input_name="SL", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*SR*/.input_name="SR", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*LB*/.input_name="LB", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*RB*/.input_name="RB", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*LT*/.input_name="L", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*RT*/.input_name="R", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*LP1*/0}, \
        (inputInfoSlot_s) {/*RP1*/0}, \
        (inputInfoSlot_s) {/*Start*/.input_name="Start", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Select*/.input_name="Select", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Home*/.input_name="Home", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*Share*/.input_name="Share", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*LP2*/0}, \
        (inputInfoSlot_s) {/*RP2*/0}, \
        (inputInfoSlot_s) {/*TP1*/0}, \
        (inputInfoSlot_s) {/*TP2*/0}, \
        (inputInfoSlot_s) {/*MISC3*/.input_name="Power", .input_type=INPUT_TYPE_DIGITAL}, \
        (inputInfoSlot_s) {/*MISC4*/0}, \
        (inputInfoSlot_s) {/*LTANALOG*/.input_name="LT", .input_type=INPUT_TYPE_HOVER}, \
        (inputInfoSlot_s) {/*RTANALOG*/.input_name="RT", .input_type=INPUT_TYPE_HOVER}, \
        (inputInfoSlot_s) {/*LX_RIGHT*/.input_name="LX+", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*LX_LEFT*/.input_name="LX-", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*LY_UP*/.input_name="LY+", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*LY_DOWN*/.input_name="LY-", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*RX_RIGHT*/.input_name="RX+", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*RX_LEFT*/.input_name="RX-", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*RY_UP*/.input_name="RY+", .input_type=INPUT_TYPE_JOYSTICK}, \
        (inputInfoSlot_s) {/*RY_DOWN*/.input_name="RY-", .input_type=INPUT_TYPE_JOYSTICK} \
    }
#endif

const inputInfoStatic_s input_static = {
    .input_info = HOJA_INPUT_SLOTS
};

#if !defined(HOJA_BLUETOOTH_PART_NUMBER)
    #define HOJA_BLUETOOTH_PART_NUMBER "N/A"
#endif

#if !defined(HOJA_BLUETOOTH_FCC_ID)
    #define HOJA_BLUETOOTH_FCC_ID "N/A"
#endif

bluetoothInfoStatic_s bluetooth_static = {
    .part_number = HOJA_BLUETOOTH_PART_NUMBER,
    .fcc_id = HOJA_BLUETOOTH_FCC_ID,
};

rgbInfoStatic_s rgb_static = {
    .rgb_groups = 0,
    .rgb_player_group = -1
};

void _rgb_static_set_names() 
{
#if defined(HOJA_RGB_CFG_PRESENT)
    const hoja_config_s *config = hoja_config_get();
    if(!config)
        return;

    const hoja_rgb_cfg_s *rgb_cfg = &config->rgb;
    rgb_static.rgb_groups = rgb_config_infer_group_count(rgb_cfg);
    rgb_static.rgb_player_group = rgb_cfg->player_group_index;

    for(int i = 0; i < RGB_MAX_GROUPS; i++)
    {
        if(!rgb_group_cfg_enabled(&rgb_cfg->groups[i]))
            continue;
        memcpy(rgb_static.rgb_group_names[i].rgb_group_name,
               rgb_cfg->groups[i].name,
               RGB_MAX_GROUP_NAME_LEN);
    }
#else
    #if !defined(HOJA_RGB_GROUPS_NUM)
        #warning "HOJA_RGB_GROUPS_NUM undefined in board_config.h. RGB features will be disabled"
        rgb_static.rgb_groups = 0;
    #else
        #if (HOJA_RGB_GROUPS_NUM > 32)
            #error "HOJA_RGB_GROUPS_NUM must be 32 or less!"
        #endif
        rgb_static.rgb_groups = HOJA_RGB_GROUPS_NUM;

        #if !defined(HOJA_RGB_GROUP_NAMES)
            #error "You must provide HOJA_RGB_GROUP_NAMES. 8 ASCII characters, 32 groups max."
        #else
            uint8_t _rgb_names[32][8] = HOJA_RGB_GROUP_NAMES;
            for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
            {
                memcpy(rgb_static.rgb_group_names[i].rgb_group_name,
                       &_rgb_names[i][0],
                       RGB_MAX_GROUP_NAME_LEN);
            }
        #endif
    #endif
#endif
}

void _analog_static_setup()
{
    const inputInfoSlot_s *slots = input_static.input_info;

    if(slots[INPUT_CODE_LT_ANALOG].input_type == INPUT_TYPE_HOVER)
        analog_static.axis_lt = 1;

    if(slots[INPUT_CODE_RT_ANALOG].input_type == INPUT_TYPE_HOVER)
        analog_static.axis_rt = 1;

    if(slots[INPUT_CODE_LX_LEFT].input_type == INPUT_TYPE_JOYSTICK)
        analog_static.axis_lx = 1;

    if(slots[INPUT_CODE_LY_UP].input_type == INPUT_TYPE_JOYSTICK)
        analog_static.axis_ly = 1;

    if(slots[INPUT_CODE_RX_LEFT].input_type == INPUT_TYPE_JOYSTICK)
        analog_static.axis_rx = 1;

    if(slots[INPUT_CODE_RY_UP].input_type == INPUT_TYPE_JOYSTICK)
        analog_static.axis_ry = 1;

    analog_static.invert_allowed = ANALOG_INVERT_ALLOWED;
}

#define BLOCK_CHUNK_MAX 32
#define BLOCK_REPORT_ID_IDX 0 // We typically don't use this
#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

static void _bluetooth_static_apply_caps(void)
{
    transport_bt_static_caps_s bt_caps = {0};

    transport_bt_static_get_caps(&bt_caps);
    bluetooth_static.bluetooth_bdr_supported = bt_caps.bdr_supported;
    bluetooth_static.bluetooth_ble_supported = bt_caps.ble_supported;
    bluetooth_static.external_update_supported = bt_caps.external_update_supported;
    bluetooth_static.wlan_supported = transport_wlan_static_supported();
}

static uint8_t _wireless_part_status_combine(uint8_t bt_status, uint8_t wlan_status)
{
    if (bt_status == WIRELESS_PART_STATUS_ERROR || wlan_status == WIRELESS_PART_STATUS_ERROR)
    {
        return WIRELESS_PART_STATUS_ERROR;
    }

    bool bt_probed = bt_status != WIRELESS_PART_STATUS_NA;
    bool wlan_probed = wlan_status != WIRELESS_PART_STATUS_NA;

    if (!bt_probed && !wlan_probed)
    {
        return WIRELESS_PART_STATUS_NA;
    }

    if (bt_probed && bt_status != WIRELESS_PART_STATUS_OK)
    {
        return WIRELESS_PART_STATUS_ERROR;
    }

    if (wlan_probed && wlan_status != WIRELESS_PART_STATUS_OK)
    {
        return WIRELESS_PART_STATUS_ERROR;
    }

    return WIRELESS_PART_STATUS_OK;
}

/** Probe wireless hardware and refresh runtime bluetooth/wlan static fields. */
static void _bluetooth_static_refresh(void)
{
    uint8_t bt_status = transport_bt_static_part_status();
    uint8_t wlan_status = transport_wlan_static_part_status();
    uint8_t overall_status = _wireless_part_status_combine(bt_status, wlan_status);

    bluetooth_static.wireless_part_status = overall_status;
    bluetooth_static.external_version_number = transport_bt_static_external_version();

    memset(bluetooth_static.part_number, 0, sizeof(bluetooth_static.part_number));
    strncpy((char *)bluetooth_static.part_number, HOJA_BLUETOOTH_PART_NUMBER,
        sizeof(bluetooth_static.part_number) - 1u);
}

/** Copy a config string into a fixed device_static field with a fallback. */
static void _device_static_copy(uint8_t *dst, size_t dst_size, const char *src, const char *fallback)
{
    const char *val = (src != NULL) ? src : fallback;
    memset(dst, 0, dst_size);
    strncpy((char *)dst, val, dst_size - 1u);
}

/** Refresh runtime device identity/URL fields from the hoja config. */
static void _device_static_refresh(void)
{
    const hoja_config_s *config = hoja_config_get();

    const char *name     = config ? config->device_name   : NULL;
    const char *maker    = config ? config->device_maker  : NULL;
    const char *manifest = config ? config->manifest_url  : NULL;
    const char *firmware = config ? config->firmware_url  : NULL;
    const char *manual   = config ? config->manual_url    : NULL;

    _device_static_copy(device_static.name,         sizeof(device_static.name),         name,     "HOJA GamePad");
    _device_static_copy(device_static.maker,        sizeof(device_static.maker),        maker,    "HHL");
    // "~" is the sentinel the app reads as "no URL".
    _device_static_copy(device_static.manifest_url, sizeof(device_static.manifest_url), manifest, "~");
    _device_static_copy(device_static.firmware_url, sizeof(device_static.firmware_url), firmware, "~");
    _device_static_copy(device_static.manual_url,   sizeof(device_static.manual_url),   manual,   "~");
}

/** Refresh runtime IMU static fields from the selected IMU driver. */
static void _imu_static_refresh(void)
{
    uint8_t channels = imu_driver_channel_count();
    imu_static.axis_gyro_a  = (channels >= 1) ? 1 : 0;
    imu_static.axis_accel_a = (channels >= 1) ? 1 : 0;
    imu_static.axis_gyro_b  = (channels >= 2) ? 1 : 0;
    imu_static.axis_accel_b = (channels >= 2) ? 1 : 0;
}

/** Refresh runtime battery static fields sourced from the hoja config/driver. */
static void _battery_static_refresh(void)
{
    const hoja_config_s *config = hoja_config_get();

    // Capacity is a generic board parameter carried in the hoja config.
    battery_static.battery_capacity_mah = config ? config->battery_capacity_mah : 0;

    // Physical battery pack code is a board parameter carried in the hoja config.
    const char *pack = (config && config->battery_part_code) ? config->battery_part_code : "N/A";
    memset(battery_static.battery_part_number, 0, sizeof(battery_static.battery_part_number));
    strncpy((char *)battery_static.battery_part_number, pack,
        sizeof(battery_static.battery_part_number) - 1u);

    // PMIC part number is supplied by the selected battery driver (weak default
    // returns NULL when no driver is compiled in).
    const char *pmic = battery_driver_part_code();
    if(pmic == NULL) pmic = "N/A";
    memset(battery_static.pmic_part_number, 0, sizeof(battery_static.pmic_part_number));
    strncpy((char *)battery_static.pmic_part_number, pmic,
        sizeof(battery_static.pmic_part_number) - 1u);

    // Fuel gauge part number is supplied by the selected fuel gauge driver
    // (weak default returns NULL when no driver is compiled in).
    const char *fgpart = fuelgauge_driver_part_code();
    if(fgpart == NULL) fgpart = "N/A";
    memset(battery_static.fuelgauge_part_number, 0, sizeof(battery_static.fuelgauge_part_number));
    strncpy((char *)battery_static.fuelgauge_part_number, fgpart,
        sizeof(battery_static.fuelgauge_part_number) - 1u);
}

void static_config_init()
{
    _rgb_static_set_names();
    _analog_static_setup();
    _bluetooth_static_apply_caps();
    _battery_static_refresh();
}

uint8_t _serdata[64] = {0};
void _serialize_static_block(static_block_t block, uint8_t *data , uint32_t size, setting_callback_t cb)
{
    memset(_serdata, 0, 64);
    _serdata[0] = WEBUSB_ID_READ_STATIC_BLOCK;
    _serdata[BLOCK_TYPE_IDX] = (uint8_t) block;

    if(size <= BLOCK_CHUNK_MAX)
    {
        _serdata[BLOCK_CHUNK_SIZE_IDX] = (uint8_t) size;
        _serdata[BLOCK_CHUNK_PART_IDX] = 0;
        memcpy(&(_serdata[BLOCK_CHUNK_BEGIN_IDX]), data, size);
        cb(_serdata, size+BLOCK_CHUNK_HEADER_SIZE);
    }
    else 
    {
        uint32_t remaining = size;
        uint8_t idx = 0;
        while(remaining>0 && idx<200)
        {
            uint32_t loop_size = remaining <= BLOCK_CHUNK_MAX ? remaining : BLOCK_CHUNK_MAX;
            _serdata[BLOCK_CHUNK_SIZE_IDX] = loop_size;
            _serdata[BLOCK_CHUNK_PART_IDX] = idx;
            memcpy(&(_serdata[BLOCK_CHUNK_BEGIN_IDX]), &(data[idx*BLOCK_CHUNK_MAX]), loop_size);
            cb(_serdata, loop_size+BLOCK_CHUNK_HEADER_SIZE);
            
            remaining -= loop_size;
            idx += 1;
        }
    }

    // Here we send the chunk completion byte
    _serdata[BLOCK_CHUNK_SIZE_IDX] = 0;
    _serdata[BLOCK_CHUNK_PART_IDX] = 0xFF;
    cb(_serdata, BLOCK_CHUNK_HEADER_SIZE);
}

void static_config_read_all_blocks(setting_callback_t cb)
{
    
}

void static_config_read_block(static_block_t block, setting_callback_t cb)
{
    switch(block)
    {
        default:
        break;

        case STATIC_BLOCK_DEVICE:
            _device_static_refresh();
            _serialize_static_block(block, (uint8_t *) &device_static, STATINFO_DEVICE_BLOCK_SIZE, cb);
        break;

        case STATIC_BLOCK_INPUT:
            _serialize_static_block(block, (uint8_t *) &input_static, STATINFO_DEVICE_INPUT_SIZE, cb);
        break;

        case STATIC_BLOCK_ANALOG:
            _serialize_static_block(block, (uint8_t *) &analog_static, STATINFO_ANALOG_SIZE, cb);
        break;

        case STATIC_BLOCK_HAPTIC:
            _serialize_static_block(block, (uint8_t *) &haptic_static, STATINFO_HAPTIC_SIZE, cb);
        break;

        case STATIC_BLOCK_IMU:
            _imu_static_refresh();
            _serialize_static_block(block, (uint8_t *) &imu_static, STATINFO_IMU_SIZE, cb);
        break;

        case STATIC_BLOCK_BATTERY:
            battery_status_s batstat = {0};
            fuelgauge_status_s fgstat = {0};

            battery_get_status(&batstat);
            fuelgauge_get_status(&fgstat);

            if(fuelgauge_driver_part_code() == NULL)
                battery_static.fuelgauge_status = 0; // No fuel gauge driver compiled in
            else
                battery_static.fuelgauge_status = fgstat.connected ? 2 : 1;

            if(battery_driver_part_code() == NULL)
                battery_static.pmic_status = 0; // No PMIC driver compiled in
            else
                battery_static.pmic_status = batstat.connected ? 2 : 1;

            _serialize_static_block(block, (uint8_t *) &battery_static, STATINFO_BATTERY_SIZE, cb);
        break;

        case STATIC_BLOCK_BLUETOOTH:
            _bluetooth_static_refresh();
            _serialize_static_block(block, (uint8_t *) &bluetooth_static, STATINFO_BLUETOOTH_SIZE, cb);
        break;

        case STATIC_BLOCK_RGB:
            _serialize_static_block(block, (uint8_t *) &rgb_static, STATINFO_RGB_SIZE, cb);
        break;
    }
}