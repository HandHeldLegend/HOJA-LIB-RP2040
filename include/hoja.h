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

// SInput descriptor enums (sinput_sdl_gamepad_type_t / sinput_sdl_face_style_t).
#include "sinput_lib_types.h"

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

// ---- IMU driver config type, shaped by the HOJA_IMU_DRIVER gate ----
#if defined(HOJA_IMU_DRIVER) && (HOJA_IMU_DRIVER != 0)
  #if (HOJA_IMU_DRIVER == IMU_DRIVER_LSM6DSR)
    #include "drivers/imu/lsm6dsr.h"
    typedef lsm6dsr_cfg_s hoja_imu_cfg_t;
    #define HOJA_IMU_CFG_PRESENT 1
  #elif (HOJA_IMU_DRIVER == IMU_DRIVER_CUSTOM)
    // Override hook: board_config.h includes the custom driver's header and
    // #defines HOJA_IMU_CFG_TYPE to its config struct type. The custom driver
    // provides the strong imu_driver_* definitions declared in input/imu.h.
    typedef HOJA_IMU_CFG_TYPE hoja_imu_cfg_t;
    #define HOJA_IMU_CFG_PRESENT 1
  #endif
#endif

// ---- Haptics driver config type, shaped by the HOJA_HAPTICS_DRIVER gate ----
// The LRA and ERM HALs need different parameters (LRA is dual-channel + PCM
// tuned; ERM is single-channel with an optional brake line), so each HAL header
// defines its own config struct and we select it here.
#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER != 0)
  #if (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
    #include "hal/lra_hal.h"
    typedef lra_hal_cfg_s hoja_haptics_cfg_t;
    #define HOJA_HAPTICS_CFG_PRESENT 1
  #elif (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_ERM_HAL)
    #include "hal/erm_hal.h"
    typedef erm_hal_cfg_s hoja_haptics_cfg_t;
    #define HOJA_HAPTICS_CFG_PRESENT 1
  #endif
#endif

// ---- Joybus (N64 + GameCube) shared line config ----
// N64 and GameCube are never active at once and always drive the same physical
// data line, so they share this single config block. The PIO block + state
// machine are allocated dynamically at init, so only the data GPIO is needed.
typedef struct
{
    uint8_t data_pin; // shared N64/GameCube joybus data GPIO
} hoja_joybus_cfg_s;

// ---- NESBUS (NES/SNES serial) config ----
// The PIO block + state machines are allocated dynamically at init.
typedef struct
{
    uint8_t data_pin;
    uint8_t clock_pin;
    uint8_t latch_pin;
} hoja_nesbus_cfg_s;

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

#if defined(HOJA_IMU_CFG_PRESENT)
    // IMU config. Type is chosen by the HOJA_IMU_DRIVER gate.
    hoja_imu_cfg_t imu;
#endif

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

    // ---- Device identity + app-facing URLs ----
    // Surfaced to the config tool / update flow. NULL fields fall back to a
    // sane library default. device_name also drives the WLAN dongle HID name.
    const char *device_name;   // e.g. "GCU-2"
    const char *device_maker;  // e.g. "HHL"
    const char *manifest_url;  // firmware-update manifest (NULL disables updates)
    const char *firmware_url;  // firmware image URL
    const char *manual_url;    // documentation URL

    // ---- USB identity ----
    // Used for the USB device descriptor and advertised over WLAN/SInput.
    // 0 falls back to the library default (Raspberry Pi VID / Hoja PID).
    uint16_t usb_vid;
    uint16_t usb_pid;

    // ---- SInput device descriptor ----
    // gamepad_subtype is a board-specific raw byte; the other two use the
    // SInput library enums for legibility.
    struct
    {
        sinput_sdl_gamepad_type_t gamepad_type;
        sinput_sdl_face_style_t   face_buttons_style;
        uint8_t                   gamepad_subtype;
    } sinput;

    // Physical face-button (SEWN) layout for boot mode selection:
    // SEWN_LAYOUT_ABXY (Xbox), SEWN_LAYOUT_BAYX (Nintendo), SEWN_LAYOUT_AXBY (GC).
    uint8_t sewn_layout;

    // ---- Tournament-lockout macro ----
    // When enabled, holding tourney_macro_code toggles tournament mode, which
    // masks dpad / extra face inputs while hovering. tourney_macro_code is a
    // mapper input (INPUT_CODE_*) that selects which physical button triggers it.
    bool                tourney_macro_enable;
    mapper_input_code_t tourney_macro_code;

#if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER) || defined(HOJA_TRANSPORT_JOYBUSGC_DRIVER)
    // Shared N64/GameCube joybus line. Present when either joybus transport gate
    // is declared in board_config.h.
    hoja_joybus_cfg_s joybus;
#endif

#if defined(HOJA_TRANSPORT_NESBUS_DRIVER)
    // NESBUS line. Present when the NESBUS transport gate is declared.
    hoja_nesbus_cfg_s nesbus;
#endif

#if defined(HOJA_HAPTICS_CFG_PRESENT)
    // Haptics config. Type is chosen by the HOJA_HAPTICS_DRIVER gate
    // (lra_hal_cfg_s or erm_hal_cfg_s).
    hoja_haptics_cfg_t haptics;
#endif
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
