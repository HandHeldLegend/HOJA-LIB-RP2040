#ifndef LAMP_ARRAY_H
#define LAMP_ARRAY_H

#include "board_config.h"
#include "class/hid/hid.h"

// LampArray Attributes
#if defined(HOJA_PRODUCT_WIDTH) && defined(HOJA_PRODUCT_HEIGHT) && defined(HOJA_PRODUCT_DEPTH)
#define LAMPARRAY_WIDTH             HOJA_PRODUCT_WIDTH
#define LAMPARRAY_HEIGHT            HOJA_PRODUCT_HEIGHT
#define LAMPARRAY_DEPTH             HOJA_PRODUCT_DEPTH
#else
#define LAMPARRAY_WIDTH             200000
#define LAMPARRAY_HEIGHT            150000
#define LAMPARRAY_DEPTH             100000
#endif
#define LAMPARRAY_UPDATE_INTERVAL   10000   // 10 ms update interval
#define LAMP_UPDATE_RATE_MS         LAMPARRAY_UPDATE_INTERVAL / 1000
#define LAMPARRAY_LAMP_COUNT        HOJA_RGB_GROUPS_NUM

typedef enum {
    LAMP_ARRAY_KIND_UNDEFINED       = 0x0,
    LAMP_ARRAY_KIND_KEYBOARD        = 0x1,
    LAMP_ARRAY_KIND_MOUSE           = 0x2,
    LAMP_ARRAY_KIND_GAME_CONTROLLER = 0x3,
    LAMP_ARRAY_KIND_PERIPHERAL      = 0x4,
    LAMP_ARRAY_KIND_SCENE           = 0x5,
    LAMP_ARRAY_KIND_NOTIFICATION    = 0x6,
    LAMP_ARRAY_KIND_CHASSIS         = 0x7,
    LAMP_ARRAY_KIND_WEARABLE        = 0x8,
    LAMP_ARRAY_KIND_FURNITURE       = 0x9,
    LAMP_ARRAY_KIND_ART             = 0xA,
    _LAMP_ARRAY_KIND_RESERVED       = 0xB,
    LAMP_ARRAY_KIND_VENDOR_DEFINED  = 0x10000
} LampArrayKind;

typedef enum {
    REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES    = 0xC8,
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST  = 0xC9,
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE = 0xCA,
    REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE        = 0xCB,
    REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE        = 0xCC,
    REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL       = 0xCD
} LampArrayReportID;

typedef enum {
    LAMP_PURPOSE_ACCENT         = 0x01,
    LAMP_PURPOSE_CONTROL        = 0x02,
    LAMP_PURPOSE_BRANDING       = 0x04,
    LAMP_PURPOSE_STATUS         = 0x08,
    LAMP_PURPOSE_ILLUMINATION   = 0x10,
    LAMP_PURPOSE_PRESENTATION   = 0x20,
    _LAMP_PURPOSE_RESERVED      = 0x40,
    LAMP_PURPOSE_VENDOR         = 0x10000
} LampPurposeFlags;

typedef enum {
    LAMP_UPDATE_FLAG_COMPLETE = 0x01,
    _LAMP_UPDATE_FLAG_RESERVED = 0x02
} LampUpdateFlags;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue; 
    uint8_t intensity; 
} __attribute__((packed, aligned(1))) LampColor;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} __attribute__((packed, aligned(1))) Position;

typedef struct {
    uint16_t lamp_count;
    
    uint32_t width;
    uint32_t height;
    uint32_t depth;

    uint32_t lamp_array_kind;
    uint32_t min_update_interval;
} __attribute__((packed, aligned(1))) LampArrayAttributesReport;

typedef struct {
    uint16_t lamp_id;
} __attribute__((packed, aligned(1))) LampAttributesRequestReport;

typedef struct {
    uint16_t lamp_id;

    Position lamp_position;

    uint32_t update_latency;
    uint32_t lamp_purposes;

    uint8_t red_level_count;
    uint8_t green_level_count;
    uint8_t blue_level_count;
    uint8_t intensity_level_count;

    uint8_t is_programmable;
    uint8_t input_binding;
} __attribute__((packed, aligned(1))) LampAttributesResponseReport;

typedef struct {
    uint8_t lamp_count;
    uint8_t flags;
    uint16_t lamp_ids[8];

    LampColor colors[8];
} __attribute__((packed, aligned(1))) LampMultiUpdateReport;

typedef struct {
    uint8_t flags;
    uint16_t lamp_id_start;
    uint16_t lamp_id_end;

    LampColor color;
} __attribute__((packed, aligned(1))) LampRangeUpdateReport;

typedef struct {
    uint8_t autonomous_mode;
} __attribute__((packed, aligned(1))) LampArrayControlReport;

#ifdef HOJA_RGB_GROUP_POSITIONS
static Position lamp_positions[LAMPARRAY_LAMP_COUNT] = HOJA_RGB_GROUP_POSITIONS;
#else
static Position lamp_positions[LAMPARRAY_LAMP_COUNT] = {0};
#endif

uint16_t handle_lamparray_get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer);
void handle_lamparray_set_report(uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);


// HID Lighting and Illumination Report Descriptor Template
// - 1st parameter is report id (required)
//   Creates 6 report ids for lighting HID usages in the following order:
//     report_id+0: HID_USAGE_LIGHTING_LAMP_ARRAY_ATTRIBUTES_REPORT
//     report_id+1: HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_REQUEST_REPORT
//     report_id+2: HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_RESPONSE_REPORT
//     report_id+3: HID_USAGE_LIGHTING_LAMP_MULTI_UPDATE_REPORT
//     report_id+4: HID_USAGE_LIGHTING_LAMP_RANGE_UPDATE_REPORT
//     report_id+5: HID_USAGE_LIGHTING_LAMP_ARRAY_CONTROL_REPORT
#define HOJA_HID_REPORT_DESC_LIGHTING(report_id) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_LIGHTING_AND_ILLUMINATION ),\
  HID_USAGE      ( HID_USAGE_LIGHTING_LAMP_ARRAY            ),\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION               ),\
    /* Lamp Array Attributes Report */ \
    HID_REPORT_ID (report_id                                    ) \
    HID_USAGE ( HID_USAGE_LIGHTING_LAMP_ARRAY_ATTRIBUTES_REPORT ),\
    HID_COLLECTION ( HID_COLLECTION_LOGICAL                     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_COUNT                          ),\
      HID_LOGICAL_MIN   ( 0                                                      ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                               ),\
      HID_REPORT_SIZE   ( 16                                                     ),\
      HID_REPORT_COUNT  ( 1                                                      ),\
      HID_FEATURE       ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE             ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_WIDTH_IN_MICROMETERS   ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_HEIGHT_IN_MICROMETERS  ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_DEPTH_IN_MICROMETERS   ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ARRAY_KIND                     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_MIN_UPDATE_INTERVAL_IN_MICROSECONDS ),\
      HID_LOGICAL_MIN   ( 0                                                      ),\
      HID_LOGICAL_MAX_N ( 2147483647, 3                                          ),\
      HID_REPORT_SIZE   ( 32                                                     ),\
      HID_REPORT_COUNT  ( 5                                                      ),\
      HID_FEATURE       ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE             ),\
    HID_COLLECTION_END ,\
    /* Lamp Attributes Request Report */ \
    HID_REPORT_ID       ( report_id + 1                                     ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_REQUEST_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID             ),\
      HID_LOGICAL_MIN   ( 0                                      ),\
      HID_LOGICAL_MAX_N ( 65535, 3                               ),\
      HID_REPORT_SIZE   ( 16                                     ),\
      HID_REPORT_COUNT  ( 1                                      ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END ,\
    /* Lamp Attributes Response Report */ \
    HID_REPORT_ID       ( report_id + 2                                      ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_RESPONSE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                             ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID                        ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                          ),\
      HID_REPORT_SIZE   ( 16                                                ),\
      HID_REPORT_COUNT  ( 1                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_X_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_Y_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_Z_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_UPDATE_LATENCY_IN_MICROSECONDS ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_PURPOSES                  ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 2147483647, 3                                     ),\
      HID_REPORT_SIZE   ( 32                                                ),\
      HID_REPORT_COUNT  ( 5                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_LEVEL_COUNT                ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_LEVEL_COUNT              ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_LEVEL_COUNT               ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_LEVEL_COUNT          ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_IS_PROGRAMMABLE                ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INPUT_BINDING                  ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 255, 2                                            ),\
      HID_REPORT_SIZE   ( 8                                                 ),\
      HID_REPORT_COUNT  ( 6                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
    HID_COLLECTION_END ,\
    /* Lamp Multi-Update Report */ \
    HID_REPORT_ID       ( report_id + 3                               ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_MULTI_UPDATE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_COUNT               ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS        ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX   ( 8                                           ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 2                                           ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID                  ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                    ),\
      HID_REPORT_SIZE   ( 16                                          ),\
      HID_REPORT_COUNT  ( 8                                           ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 255, 2                                      ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 32                                          ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
    HID_COLLECTION_END ,\
    /* Lamp Range Update Report */ \
    HID_REPORT_ID       ( report_id + 4 ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_RANGE_UPDATE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS        ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX   ( 8                                           ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 1                                           ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID_START            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID_END              ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                    ),\
      HID_REPORT_SIZE   ( 16                                          ),\
      HID_REPORT_COUNT  ( 2                                           ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 255, 2                                      ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 4                                           ),\
      HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
    HID_COLLECTION_END ,\
    /* Lamp Array Control Report */ \
    HID_REPORT_ID      ( report_id + 5                                ) \
    HID_USAGE          ( HID_USAGE_LIGHTING_LAMP_ARRAY_CONTROL_REPORT ),\
    HID_COLLECTION     ( HID_COLLECTION_LOGICAL                       ),\
      HID_USAGE        ( HID_USAGE_LIGHTING_AUTONOMOUS_MODE     ),\
      HID_LOGICAL_MIN  ( 0                                      ),\
      HID_LOGICAL_MAX  ( 1                                      ),\
      HID_REPORT_SIZE  ( 8                                      ),\
      HID_REPORT_COUNT ( 1                                      ),\
      HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END ,\
  HID_COLLECTION_END \

#endif

