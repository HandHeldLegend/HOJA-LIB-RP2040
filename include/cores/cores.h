#ifndef CORES_CENTRAL_H
#define CORES_CENTRAL_H
#include <stdint.h>

#include "hoja_shared_types.h"
#include <hoja_usb.h>

typedef enum 
{
    CORE_REPORTFORMAT_UNDEFINED = -1,
    CORE_REPORTFORMAT_SWPRO,
    CORE_REPORTFORMAT_XINPUT,
    CORE_REPORTFORMAT_SINPUT,
    CORE_REPORTFORMAT_SLIPPI,
    CORE_REPORTFORMAT_SNES,
    CORE_REPORTFORMAT_N64,
    CORE_REPORTFORMAT_GAMECUBE
} core_reportformat_t;

typedef struct
{
    uint8_t *hid_report_descriptor;
    uint16_t hid_report_descriptor_len;
    uint8_t *config_descriptor;
    uint16_t config_descriptor_len;
    uint16_t vid;
    uint16_t pid;
    char name[64];
    usb_device_descriptor_t *device_descriptor;
} core_hid_device_t;

#define CORE_REPORT_DATA_LEN 64
typedef struct
{
    core_reportformat_t reportformat;
    uint8_t size;
    uint8_t data[CORE_REPORT_DATA_LEN];
} core_report_s;

typedef bool (*core_generate_report_t)(core_report_s *out);
typedef void (*core_report_tunnel_t)(uint8_t *data, uint16_t len);
typedef void (*core_transport_task_t)(uint64_t timestamp);
typedef core_hid_device_t* (*core_get_hid_device_t)(void);

typedef struct 
{
    uint8_t                 transport_type;
    uint8_t                 transport_dev_mac[6];
    uint8_t                 transport_host_mac[6];
    core_transport_task_t   transport_task;
    core_reportformat_t     core_report_format;
    uint16_t                core_pollrate_us; // Transport methods may or may not respect this value
    core_generate_report_t  core_report_generator; // Get generated report data from this
    core_report_tunnel_t    core_report_tunnel;    // Where incoming reports should be sent
    core_hid_device_t*      hid_device; // HID device info
} core_params_s;

bool core_is_mac_blank(uint8_t mac[6]);

bool core_get_generated_report(core_report_s *out);
void core_report_tunnel_cb(uint8_t *data, uint16_t len);

void core_deinit();
bool core_init(gamepad_mode_t mode, gamepad_transport_t transport, bool pair);
void core_task(uint64_t timestamp);

#endif