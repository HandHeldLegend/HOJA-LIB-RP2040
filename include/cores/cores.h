#ifndef CORES_CENTRAL_H
#define CORES_CENTRAL_H
#include <stdint.h>

#include "hoja_shared_types.h"

typedef enum 
{
    CORE_FORMAT_UNDEFINED = -1,
    CORE_FORMAT_SWPRO,
    CORE_FORMAT_XINPUT,
    CORE_FORMAT_SINPUT,
    CORE_FORMAT_GCUSB,
    CORE_FORMAT_SNES,
    CORE_FORMAT_N64,
    CORE_FORMAT_GAMECUBE
} core_reportformat_t;

typedef struct
{
    core_reportformat_t format;
    uint8_t size;
    uint8_t data[64];
} core_report_s;

typedef bool (*core_generate_report_t)(core_report_s *out);
typedef void (*core_report_tunnel_t)(uint8_t *data, uint16_t len);
typedef void (*core_transport_task_t)(uint64_t timestamp);

typedef struct 
{
    uint8_t                 gamepad_mode;
    uint8_t                 gamepad_transport;
    core_transport_task_t   transport_task;
    core_reportformat_t     report_format;
    core_generate_report_t  report_generator; // Get generated report data from this
    core_report_tunnel_t    report_tunnel;    // Where incoming reports should be sent
} core_params_s;

void core_get_generated_report(core_report_s *out);
void core_report_tunnel_cb(uint8_t *data, uint16_t len);

void core_init();
void core_task(uint64_t timestamp);

#endif