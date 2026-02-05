#ifndef CORES_CENTRAL_H
#define CORES_CENTRAL_H
#include <stdint.h>

#include "hoja_shared_types.h"

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
    ext_tusb_desc_device_t *device_descriptor;
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
    uint8_t                 gamepad_mode;
    uint8_t                 gamepad_transport;
    uint16_t                pollrate_us;
    core_transport_task_t   transport_task;
    core_reportformat_t     report_format;
    core_generate_report_t  report_generator; // Get generated report data from this
    core_report_tunnel_t    report_tunnel;    // Where incoming reports should be sent
    core_hid_device_t*      hid_device; // HID device info
} core_params_s;

bool core_get_generated_report(core_report_s *out);
void core_report_tunnel_cb(uint8_t *data, uint16_t len);

bool core_init(gamepad_mode_t mode, gamepad_transport_t transport);
void core_task(uint64_t timestamp);

#endif