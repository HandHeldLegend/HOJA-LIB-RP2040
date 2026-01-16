#include "devices/lamp_array.h"
#include "class/hid/hid.h"
#include "settings_shared_types.h"
#include "devices/rgb.h"
#include "devices/animations/anm_handler.h"
#include "devices/animations/anm_external.h"
#include <stdint.h>

static uint16_t currentLampId = 0;

uint16_t _attr_report(uint8_t* buffer) {
    LampArrayAttributesReport report = {
        LAMPARRAY_LAMP_COUNT,
        LAMPARRAY_WIDTH,
        LAMPARRAY_HEIGHT,
        LAMPARRAY_DEPTH,
        LAMP_ARRAY_KIND_GAME_CONTROLLER,
        LAMPARRAY_UPDATE_INTERVAL
    };

    memcpy(buffer, &report, sizeof(LampArrayAttributesReport));
    return sizeof(LampArrayAttributesReport);
}

uint16_t _attr_res_report(uint8_t* buffer) {
    LampAttributesResponseReport report = {
        currentLampId,                   // LampId
        lamp_positions[currentLampId],   // Lamp position
        LAMPARRAY_UPDATE_INTERVAL,       // Lamp update interval
        LAMP_PURPOSE_CONTROL,            // Lamp purpose
        255,                             // Red level count
        255,                             // Blue level count
        255,                             // Green level count
        1,                               // Intensity
        1,                               // Is Programmable
        0                                // InputBinding
    };

    memcpy(buffer, &report, sizeof(LampAttributesResponseReport));
    currentLampId = (currentLampId + 1) % LAMPARRAY_LAMP_COUNT;

    return sizeof(LampAttributesResponseReport);
}

void _attr_req_report(const uint8_t* buffer) {
    LampAttributesRequestReport* report = (LampAttributesRequestReport*) buffer;
    currentLampId = report->lamp_id < LAMPARRAY_LAMP_COUNT ? report->lamp_id : 0;
}

void _update_multi_report(const uint8_t* buffer) {
    LampMultiUpdateReport* report = (LampMultiUpdateReport*) buffer;
    for (int i = 0; i < report->lamp_count; i++) {
        LampColor lamp_color = report->colors[i];
        rgb_s color = {
            .r = lamp_color.red,
            .g = lamp_color.green,
            .b = lamp_color.blue,
            .padding = 0
        };
        anm_external_queue_rgb_group(report->lamp_ids[i], color);
    }
    bool update_complete = report->flags & LAMP_UPDATE_FLAG_COMPLETE;
    if (update_complete)
        anm_external_dequeue();
}

void _update_range_report(const uint8_t* buffer) {
    LampRangeUpdateReport* report = (LampRangeUpdateReport*) buffer;
    LampColor lamp_color = report->color;
    rgb_s color = {
        .r = lamp_color.red,
        .g = lamp_color.green,
        .b = lamp_color.blue,
        .padding = 0
    };
    anm_external_queue_rgb_group_range(report->lamp_id_start, report->lamp_id_end, color);
    bool update_complete = report->flags & LAMP_UPDATE_FLAG_COMPLETE;
    if (update_complete)
        anm_external_dequeue();
}

void _control_report(const uint8_t* buffer) {
    LampArrayControlReport* report = (LampArrayControlReport*) buffer;
    if (report->autonomous_mode) {
        rgb_init(-1, -1);
    } else {
        rgb_init(RGB_ANIM_EXTERNAL, RGB_BRIGHTNESS_MAX);
    }
}

uint16_t handle_lamparray_get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer)
{
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

    if (report_type != HID_REPORT_TYPE_FEATURE)
        return 0;

    switch (report_id) {
        case REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES:
            return _attr_report(buffer);
            break;
        case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE:
            return _attr_res_report(buffer);
            break;
        default:
            break;
    }

    #endif
    
    return 0;
}

void handle_lamparray_set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

    if (bufsize == 0 || report_type == HID_REPORT_TYPE_INVALID)
        return;

    if (!report_id && report_type == HID_REPORT_TYPE_OUTPUT) {
        report_id = buffer[0];
        buffer++;
        bufsize--;
    }

    switch (report_id) {
        case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST:
            _attr_req_report(buffer);
            break;
        case REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE:
            _update_multi_report(buffer);
            break;   
        case REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE:
            _update_range_report(buffer);
            break;   
        case REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL:
            _control_report(buffer);
            break;
        default:
            break;
    }

    #endif
}

