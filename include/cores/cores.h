#ifndef CORES_CENTRAL_H
#define CORES_CENTRAL_H
#include <stdint.h>

#include "hoja_shared_types.h"
#include <hoja_usb.h>

typedef struct
{
    const uint8_t *hid_report_descriptor;
    uint16_t hid_report_descriptor_len;
    const uint8_t *config_descriptor;
    uint16_t config_descriptor_len;
    uint16_t vid;
    uint16_t pid;
    char name[32];
    const hoja_usb_device_descriptor_t *device_descriptor;
    // Optional: override the configuration descriptor's bMaxPower (in mA).
    // 0 keeps whatever the config descriptor already specifies.
    uint16_t max_power_ma;
} core_hid_device_t;

#define CORE_REPORT_DATA_LEN 64
typedef struct
{
    core_reportformat_t reportformat;
    bool reliable;
    uint8_t size;
    uint8_t data[CORE_REPORT_DATA_LEN];
} core_report_s;

typedef core_reportformat_t (*core_autodetect_result_t)(void);
typedef bool (*core_generate_report_t)(core_report_s *out);
typedef void (*core_report_tunnel_t)(const uint8_t *data, uint16_t len);
typedef void (*core_transport_task_t)(uint64_t timestamp);
typedef void (*core_task_t)(uint64_t timestamp);
typedef void (*core_gyro_task_t)(void);
typedef core_hid_device_t* (*core_get_hid_device_t)(void);

#define COREBOOT_FLAG_PAIR (0b1)
#define COREBOOT_FLAG_WLAN (0b10)
#define COREBOOT_FLAG_ALTFLASH (0b10000000)

typedef struct 
{
    uint8_t                 transport_type;
    uint8_t                 transport_dev_mac[6];
    uint8_t                 transport_host_mac[6];
    core_task_t             core_task;
    core_transport_task_t   transport_task;
    core_reportformat_t     core_report_format;
    uint16_t                core_pollrate_us; // Transport methods may or may not respect this value
    core_generate_report_t  core_report_generator; // Get generated report data from this
    core_report_tunnel_t    core_report_tunnel;    // Where incoming OUTPUT reports should be sent
    const core_hid_device_t*      hid_device; // HID device info
    uint16_t                core_boot_flags; // See COREBOOT_FLAG_ types
} core_params_s;


core_reportformat_t core_current_reportformat(void);
core_params_s* core_current_params();
bool core_is_mac_blank(uint8_t mac[6]);

rgb_s core_current_color_get(void);
bool core_get_generated_report(core_report_s *out);
void core_report_tunnel_cb(const uint8_t *data, uint16_t len);

void core_deinit();
bool core_init(void);
void core_task(uint64_t now_us);

// gamepadConfig_s.gamepad_default_mode is stored as a core_reportformat_t value (0..6).
static inline core_reportformat_t core_reportformat_from_default(uint8_t stored)
{
    if (stored >= (uint8_t)CORE_REPORTFORMAT_MAX)
        return CORE_REPORTFORMAT_SWPRO;
    return (core_reportformat_t)stored;
}

#endif