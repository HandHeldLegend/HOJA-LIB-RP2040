#ifndef HOJA_H
#define HOJA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hoja_shared_types.h"
#include "input_shared_types.h"
#include "devices_shared_types.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "input/analog.h"
#include "utilities/callback.h"

// board_config.h is the driver *gate*: it selects WHICH drivers are used
// (e.g. HOJA_BATTERY_DRIVER = BATTERY_DRIVER_BQ25180). The board's main.c still
// owns each driver's *configuration*, supplied through hoja_config_s below.
// The gate also shapes hoja_config_s: the embedded driver-config member takes
// on the selected driver's config struct type, so missing/zeroed fields are
// caught at the board's designated-initializer site (and validated at init).
#include "board_config.h"
#include "driver_define_helper.h"

// ---- Battery driver config type, shaped by the HOJA_BATTERY_DRIVER gate ----
#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER != 0)
  #if (HOJA_BATTERY_DRIVER == BATTERY_DRIVER_BQ25180)
    #include "drivers/battery/bq25180.h"
    typedef bq25180_cfg_s hoja_battery_cfg_t;
    #define HOJA_BATTERY_CFG_PRESENT 1
  #elif (HOJA_BATTERY_DRIVER == BATTERY_DRIVER_CUSTOM)
    // Override hook: board_config.h must include the custom driver's header and
    // #define HOJA_BATTERY_CFG_TYPE to that driver's config struct type. The
    // custom driver (compiled with the board) provides the strong
    // battery_driver_* definitions declared in devices/battery.h.
    typedef HOJA_BATTERY_CFG_TYPE hoja_battery_cfg_t;
    #define HOJA_BATTERY_CFG_PRESENT 1
  #endif
#endif

// ---- Fuel gauge driver config type, shaped by the HOJA_FUELGAUGE_DRIVER gate ----
// Some fuel gauge drivers carry no per-board config (e.g. the ESP32 gauge, whose
// state is pushed in via esp32hoja_fuelgauge_report); those define no config
// member at all (HOJA_FUELGAUGE_CFG_PRESENT stays undefined).
#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER != 0)
  #if (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_BQ27621G1)
    #include "drivers/fuelgauge/bq27621g1.h"
    typedef bq27621g1_cfg_s hoja_fuelgauge_cfg_t;
    #define HOJA_FUELGAUGE_CFG_PRESENT 1
  #elif (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ADC)
    #include "drivers/fuelgauge/adc_fuelgauge.h"
    typedef adc_fuelgauge_cfg_s hoja_fuelgauge_cfg_t;
    #define HOJA_FUELGAUGE_CFG_PRESENT 1
  #elif (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ESP32)
    // No config: state is supplied at runtime via esp32hoja_fuelgauge_report().
  #elif (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_CUSTOM)
    // Override hook: board_config.h includes the custom driver's header and
    // #defines HOJA_FUELGAUGE_CFG_TYPE to its config struct type. The custom
    // driver provides the strong fuelgauge_driver_* definitions.
    typedef HOJA_FUELGAUGE_CFG_TYPE hoja_fuelgauge_cfg_t;
    #define HOJA_FUELGAUGE_CFG_PRESENT 1
  #endif
#endif

// Per-transport capability flags. Set by name in the board's hoja_config_s
// (no bit-packing — clarity over space). transport_init() only attempts a
// transport whose flag is true here. Note: the HOJA_TRANSPORT_*_DRIVER macros
// in board_config.h still exist, but solely to compile-gate each transport's
// platform HAL implementation; the board's *selection* lives here.
typedef struct
{
    bool usb;
    bool bluetooth;
    bool wlan;
    bool nesbus;
    bool joybus_n64;
    bool joybus_gc;
} hoja_transport_support_s;

// Board-supplied driver-configuration bundle.
// Boards populate this (typically a const instance) and pass it to hoja_init().
// The shape adapts to the board's selected drivers (see the gate-driven typedef
// above); the active driver reads its config straight from here at init.
typedef struct
{
#if defined(HOJA_BATTERY_CFG_PRESENT)
    // Battery PMIC config. Type is chosen by the HOJA_BATTERY_DRIVER gate.
    hoja_battery_cfg_t battery;
#endif

#if defined(HOJA_FUELGAUGE_CFG_PRESENT)
    // Fuel gauge config. Type is chosen by the HOJA_FUELGAUGE_DRIVER gate.
    // (Drivers with no per-board config, e.g. ESP32, omit this member.)
    hoja_fuelgauge_cfg_t fuelgauge;
#endif

    // Which transports this board supports.
    hoja_transport_support_s supported_transports;

    // Board-specific battery pack capacity in mAh. Generic (not tied to any
    // particular PMIC/fuel-gauge driver), so it lives directly in the config.
    // Used to program the fuel gauge during init.
    uint16_t battery_capacity_mah;

    // Physical battery pack code (e.g. "BDT 903035"). Board-specific; surfaced
    // to the config tool. NULL falls back to "N/A". (Distinct from the PMIC
    // part code, which the battery driver supplies via battery_driver_part_code.)
    const char *battery_part_code;

    // Optional override for the SOC (%) at/below which an unplugged device
    // begins critical shutdown. 0 uses the library default (5%).
    uint8_t battery_shutdown_percent;
} hoja_config_s;

const hoja_config_s *hoja_config_get(void);

void cb_hoja_shutdown();
bool cb_hoja_boot(boot_input_s *boot);
// Optional: if true, *mode_out is applied for face-button mode selection (use GAMEPAD_MODE_LOAD
// to defer to stored default). If false, the library uses digital and/or built-in analog face logic.
bool cb_hoja_boot_custom_face_mode(const mapper_input_s *in, gamepad_mode_t *mode_out);
uint16_t cb_hoja_read_battery();
// Accessible as a uint16_t array of size 4 only
void cb_hoja_read_joystick(uint16_t *input);
void cb_hoja_read_input(mapper_input_s *input);
void cb_hoja_init();

void hoja_set_debug_data(uint8_t data);

void hoja_set_player_number(uint8_t number);
void hoja_set_connected_status(connection_status_t status);
void hoja_set_notification_status(rgb_s color);
void hoja_set_ss_notif(rgb_s color);
void hoja_clr_ss_notif();

hoja_status_s hoja_get_status();

void hoja_restart();
void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init(const hoja_config_s *config);

#endif
