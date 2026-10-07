#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cores/cores.h"
#include "cores/core_wii.h"
#include "transport/transport.h"

#include "hoja.h"
#include "board_config.h"

#include "utilities/settings.h"
#include "hal/sys_hal.h"

#include "input/mapper.h"
#include "input/dpad.h"
#include "input/hover.h"
#include "input/imu.h"

#include "devices/rgb.h"
#include "devices/fuelgauge.h"

#include "nwii_lib.h"

// Wii console mode: Wii Remote + Nunchuk, or Wii Remote + Classic Controller Pro, over the
// RM2 (CYW43) Bluetooth HAL. The ESP32 baseband has no Wii support.
#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_HAL)

// IMU scale at the ranges the LSM6DSR runs at (8 g, 2000 dps)
#define CORE_WII_ACCEL_MG_PER_LSB   0.244f
#define CORE_WII_GYRO_DPS_PER_LSB   0.07f

// Stick aim (anything mapped to the pointer outputs): screen-halves per second at full tilt.
// Y_GAIN balances vertical against horizontal speed, and a response curve keeps small deflections
// precise.
#define CORE_WII_AIM_STICK_SPEED        1.6f
#define CORE_WII_AIM_STICK_Y_GAIN       1.3f
#define CORE_WII_AIM_STICK_LINEAR       0.3f // Share of the response that is linear (rest is squared)

// Shake: square wave on the accelerometer while the shake button is held
#define CORE_WII_SHAKE_MG               3000
#define CORE_WII_SHAKE_HALF_PERIOD_US   32000

// Upright mode: while the fused gravity's levelled Z is above FACE_UP, the reported remote Z
// acceleration never drops below Z_FLOOR_MG (see the UPRIGHT accelerometer case)
#define CORE_WII_FACE_UP                0.5f
#define CORE_WII_ACCEL_Z_FLOOR_MG       300

// Analog trigger level (0..4095) where the Classic Controller's L/R click
#define CORE_WII_TRIGGER_CLICK          3900

// A power button press shorter than this toggles the extension. Holding it longer is left to
// the shutdown macro.
#define CORE_WII_POWER_TAP_US           800000

static const char _wii_name[] = "Nintendo RVL-CNT-01";

static core_hid_device_t _wii_hid_device = {
    .config_descriptor = NULL,
    .config_descriptor_len = 0,
    .hid_report_descriptor = NULL,
    .hid_report_descriptor_len = 0,
    .device_descriptor = NULL,
    .vid = NWII_HID_VID,
    .pid = NWII_HID_PID,
};

// Pointer aim from gyro + accelerometer fusion (NWII-LIB-HID's aim helper)
static nwii_aim_s _wii_aim = {0};
static uint64_t   _wii_aim_last_us = 0;

static inline float _core_wii_clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

// Takes int32 so negated samples (-INT16_MIN) cannot overflow
static inline int16_t _core_wii_mg(int32_t raw)
{
    float mg = (float)raw * CORE_WII_ACCEL_MG_PER_LSB;
    return (int16_t)_core_wii_clampf(mg, -32000.0f, 32000.0f);
}

static inline float _core_wii_stick_norm(uint16_t axis)
{
    return ((float)axis - 2048.0f) / 2048.0f;
}

static inline float _core_wii_stick_curve(float v)
{
    const float mag = (v < 0.0f) ? -v : v;
    return v * (CORE_WII_AIM_STICK_LINEAR + (1.0f - CORE_WII_AIM_STICK_LINEAR) * mag);
}

// Feed the IMU to the aim helper and add stick aim. HOJA's IMU frame (the same on every board)
// matches the helper's controller frame (+X left, +Y toward the player, +Z up, right-handed)
// except that its Y axis runs toward the front edge, on both sensors. Negating Y on both makes
// the frame right-handed; checked on a Wii by aiming flat, held vertical and rolled 90 degrees,
// and by cursor tilt keeping up with a roll.
#define CORE_WII_IMU_Y_SIGN         (-1.0f)

static void _core_wii_aim_update(const imu_data_s *imu, float stick_x, float stick_y, bool recenter)
{
    const uint64_t now = sys_hal_now_us();
    const float dt = (_wii_aim_last_us == 0) ? 0.0f : (float)(now - _wii_aim_last_us) / 1000000.0f;
    _wii_aim_last_us = now;

    const float gyro_dps[3] = {
        (float)imu->gx * CORE_WII_GYRO_DPS_PER_LSB,
        (float)imu->gy * CORE_WII_GYRO_DPS_PER_LSB * CORE_WII_IMU_Y_SIGN,
        (float)imu->gz * CORE_WII_GYRO_DPS_PER_LSB,
    };
    const float accel_g[3] = {
        (float)imu->ax * CORE_WII_ACCEL_MG_PER_LSB / 1000.0f,
        (float)imu->ay * CORE_WII_ACCEL_MG_PER_LSB / 1000.0f * CORE_WII_IMU_Y_SIGN,
        (float)imu->az * CORE_WII_ACCEL_MG_PER_LSB / 1000.0f,
    };

    nwii_aim_update(&_wii_aim, gyro_dps, accel_g, dt);

    if (recenter)
    {
        nwii_aim_recenter(&_wii_aim);
        return;
    }

    const float stick_dt = (dt > 0.05f) ? 0.05f : dt;
    nwii_aim_nudge(&_wii_aim,
                   _core_wii_stick_curve(stick_x) * CORE_WII_AIM_STICK_SPEED * stick_dt,
                   _core_wii_stick_curve(stick_y) * CORE_WII_AIM_STICK_SPEED * CORE_WII_AIM_STICK_Y_GAIN * stick_dt);
}

static bool _core_wii_code_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
        return false;

    return input->presses[code] || (input->inputs[code] > 0);
}

typedef enum
{
    CORE_WII_MODE_UPRIGHT,  // Wii Remote held upright, Nunchuk attachable
    CORE_WII_MODE_SIDEWAYS, // Wii Remote held sideways, Nunchuk attachable
    CORE_WII_MODE_CLASSIC,  // Classic Controller (Pro layout)
    CORE_WII_MODE_MAX,
} core_wii_mode_t;

static volatile core_wii_mode_t _wii_mode = CORE_WII_MODE_UPRIGHT;

// The extension each mode plugs in, and whether it is currently plugged in. Games that ask for an
// extension to be removed can be satisfied with the extension toggle (Capture by default).
static const nwii_extension_t _wii_mode_extension[CORE_WII_MODE_MAX] = {
    [CORE_WII_MODE_UPRIGHT]  = NWII_EXTENSION_NUNCHUK,
    [CORE_WII_MODE_SIDEWAYS] = NWII_EXTENSION_NUNCHUK,
    // The original Classic Controller: same buttons as the Pro, plus analog L/R triggers
    [CORE_WII_MODE_CLASSIC]  = NWII_EXTENSION_CLASSIC,
};

static bool _wii_mode_attached[CORE_WII_MODE_MAX] = {
    [CORE_WII_MODE_UPRIGHT]  = true,
    [CORE_WII_MODE_SIDEWAYS] = false, // Sideways games expect a bare remote
    [CORE_WII_MODE_CLASSIC]  = true,
};

static void _core_wii_apply_extension(void)
{
    nwii_api_set_extension(_wii_mode_attached[_wii_mode] ? _wii_mode_extension[_wii_mode] : NWII_EXTENSION_NONE);
}

static void _core_wii_set_mode(core_wii_mode_t mode)
{
    static const mapper_wii_profile_t profiles[CORE_WII_MODE_MAX] = {
        [CORE_WII_MODE_UPRIGHT]  = WII_PROFILE_NUNCHUK,
        [CORE_WII_MODE_SIDEWAYS] = WII_PROFILE_SIDEWAYS,
        [CORE_WII_MODE_CLASSIC]  = WII_PROFILE_CLASSIC,
    };

    _wii_mode = mode;
    mapper_set_wii_profile(profiles[mode]);
    _core_wii_apply_extension();
}

// Plug or unplug the current mode's extension on each press of the extension toggle
static void _core_wii_extension_toggle_task(bool pressed)
{
    static bool was_pressed = false;

    if (pressed && !was_pressed)
    {
        _wii_mode_attached[_wii_mode] = !_wii_mode_attached[_wii_mode];
        _core_wii_apply_extension();
        rgb_send_notification(_wii_mode_attached[_wii_mode] ? COLOR_GREEN : COLOR_RED);
    }

    was_pressed = pressed;
}

// The board's power button is whatever its ship-mode macro holds. A short tap cycles
// Upright -> Sideways -> Classic.
static void _core_wii_power_tap_task(void)
{
    static bool armed = false;
    static bool was_pressed = false;
    static uint64_t press_start_us = 0;

    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->shipping_macro_code[0] == INPUT_CODE_UNUSED)
        return;

    mapper_input_s raw = {0};
    hover_access_safe(&raw);

    bool pressed = _core_wii_code_pressed(&raw, cfg->shipping_macro_code[0]);
    if (cfg->shipping_macro_code[1] != INPUT_CODE_UNUSED)
        pressed = pressed && _core_wii_code_pressed(&raw, cfg->shipping_macro_code[1]);

    const uint64_t now = sys_hal_now_us();

    // Ignore a press that started before boot
    if (!armed)
    {
        armed = !pressed;
        was_pressed = pressed;
        return;
    }

    if (pressed && !was_pressed)
    {
        press_start_us = now;
    }
    else if (!pressed && was_pressed)
    {
        if ((now - press_start_us) < CORE_WII_POWER_TAP_US)
        {
            static const rgb_s colors[CORE_WII_MODE_MAX] = {
                [CORE_WII_MODE_UPRIGHT]  = COLOR_WHITE,
                [CORE_WII_MODE_SIDEWAYS] = COLOR_YELLOW,
                [CORE_WII_MODE_CLASSIC]  = COLOR_BLUE,
            };

            const core_wii_mode_t next = (core_wii_mode_t)((_wii_mode + 1) % CORE_WII_MODE_MAX);
            _core_wii_set_mode(next);
            rgb_send_notification(colors[next]);
        }
    }

    was_pressed = pressed;
}

static void _core_wii_shake(int16_t *x, int16_t *y, int16_t *z)
{
    const bool high = ((sys_hal_now_us() / CORE_WII_SHAKE_HALF_PERIOD_US) & 1u) != 0;
    const int16_t delta = high ? CORE_WII_SHAKE_MG : -CORE_WII_SHAKE_MG;

    *x = (int16_t)_core_wii_clampf((float)*x + delta, -32000.0f, 32000.0f);
    *y = (int16_t)_core_wii_clampf((float)*y + delta, -32000.0f, 32000.0f);
    *z = (int16_t)_core_wii_clampf((float)*z + delta, -32000.0f, 32000.0f);
}

static inline uint16_t _core_wii_stick(const mapper_input_s *input, mapper_wii_code_t neg, mapper_wii_code_t pos)
{
    return (uint16_t)_core_wii_clampf((float)mapper_joystick_concat(2048, input->inputs[neg], input->inputs[pos]), 0, 4095);
}

void nwii_api_hook_get_input(nwii_input_s *out)
{
    if (!out)
        return;

    _core_wii_power_tap_task();

    // Reads the Wii profile for the current mode (see mapper_set_wii_profile)
    mapper_input_s input = mapper_get_input();
    const bool *p = input.presses;

    imu_data_s imu = {0};
    imu_access_safe(&imu);

    // The pointer follows the gyro in every mode so the Wii Menu stays usable; anything mapped to
    // the pointer outputs nudges it as well.
    _core_wii_aim_update(&imu,
                         _core_wii_stick_norm(_core_wii_stick(&input, WII_CODE_POINTER_LEFT, WII_CODE_POINTER_RIGHT)),
                         _core_wii_stick_norm(_core_wii_stick(&input, WII_CODE_POINTER_DOWN, WII_CODE_POINTER_UP)),
                         p[WII_CODE_POINTER_RECENTER]);
    // Cursor tilt follows the controller's roll in Upright mode, where it is held like a pointing
    // remote
    nwii_aim_to_ir(&_wii_aim, out->ir, _wii_mode == CORE_WII_MODE_UPRIGHT);

    // Wii Remote buttons are live in every mode, so a Classic profile can still map Remote A for
    // the Wii Menu.
    bool dpad[4] = {p[WII_CODE_DOWN], p[WII_CODE_RIGHT], p[WII_CODE_LEFT], p[WII_CODE_UP]};
    dpad_translate_input(dpad);

    out->remote.a     = p[WII_CODE_A];
    out->remote.b     = p[WII_CODE_B];
    out->remote.one   = p[WII_CODE_ONE];
    out->remote.two   = p[WII_CODE_TWO];
    out->remote.plus  = p[WII_CODE_PLUS];
    out->remote.minus = p[WII_CODE_MINUS];
    out->remote.home  = p[WII_CODE_HOME];
    out->remote.down  = dpad[0];
    out->remote.right = dpad[1];
    out->remote.left  = dpad[2];
    out->remote.up    = dpad[3];

    _core_wii_extension_toggle_task(p[WII_CODE_EXTENSION_TOGGLE]);

    // Nunchuk inputs are live in every mode; the library only reports them while a Nunchuk is
    // attached.
    out->nunchuk.c = p[WII_CODE_C];
    out->nunchuk.z = p[WII_CODE_Z];
    out->nunchuk.stick_x = _core_wii_stick(&input, WII_CODE_NUNCHUK_X_LEFT, WII_CODE_NUNCHUK_X_RIGHT);
    out->nunchuk.stick_y = _core_wii_stick(&input, WII_CODE_NUNCHUK_Y_DOWN, WII_CODE_NUNCHUK_Y_UP);

    if (p[WII_CODE_NUNCHUK_SHAKE])
        _core_wii_shake(&out->nunchuk.accel_x, &out->nunchuk.accel_y, &out->nunchuk.accel_z);

    // Accelerometer frames, checked axis by axis against a real remote: HOJA's standardized IMU
    // frame (the same on every board) reads +X toward the gamepad's left side, +Y toward the
    // player, +Z up out of its face. The remote reads +X toward its left side, +Y toward its IR
    // camera, +Z up; that frame is mirrored (left-handed) relative to HOJA's.
    switch (_wii_mode)
    {
    default:
    case CORE_WII_MODE_UPRIGHT:
    {
        // The gamepad is the remote pointing at the screen. Tilt is measured from the pose at the
        // last recentre, so a gamepad aimed from any angle reads as a remote held level.
        // Levelled in the helper's frame (Y toward the player), which is the remote's Y negated.
        const float raw[3] = {(float)imu.ax, (float)imu.ay * CORE_WII_IMU_Y_SIGN, (float)imu.az};
        float level[3];
        nwii_aim_level_accel(&_wii_aim, raw, level);

        out->accel_x = _core_wii_mg((int32_t)level[0]);
        out->accel_y = _core_wii_mg((int32_t)level[1]);
        out->accel_z = _core_wii_mg((int32_t)level[2]);

        // The Wii reads which way up the remote is from the accelerometer to make sense of the IR
        // dots. A quick downward flick briefly cancels gravity, and with Z near zero (or below)
        // the Wii loses the remote's orientation and the cursor jumps. While the fused gravity
        // says the controller really is face up, keep Z above a floor; a controller turned over
        // still reads upside down, and X/Y pass through for games that read swings.
        float level_up[3];
        nwii_aim_level_accel(&_wii_aim, _wii_aim.up, level_up);
        if (_wii_aim.up_valid && (level_up[2] > CORE_WII_FACE_UP) && (out->accel_z < CORE_WII_ACCEL_Z_FLOOR_MG))
            out->accel_z = CORE_WII_ACCEL_Z_FLOOR_MG;
        break;
    }

    case CORE_WII_MODE_SIDEWAYS:
        // The gamepad stands in for a remote held sideways. Signs set by flat-table checks against
        // a real remote: forward/back tilt drives X, left/right tilt drives Y.
        out->accel_x = _core_wii_mg(-(int32_t)imu.ay);
        out->accel_y = _core_wii_mg(-(int32_t)imu.ax);
        out->accel_z = _core_wii_mg(imu.az);
        break;

    case CORE_WII_MODE_CLASSIC:
    {
        // The remote sits idle while the Classic Controller Pro is in use, so its accelerometer
        // keeps the resting default.
        bool cc_dpad[4] = {p[WII_CODE_CC_DOWN], p[WII_CODE_CC_RIGHT], p[WII_CODE_CC_LEFT], p[WII_CODE_CC_UP]};
        dpad_translate_input(cc_dpad);

        out->classic.a     = p[WII_CODE_CC_A];
        out->classic.b     = p[WII_CODE_CC_B];
        out->classic.x     = p[WII_CODE_CC_X];
        out->classic.y     = p[WII_CODE_CC_Y];
        // L/R are analog; like the original Classic Controller they click at the end of travel
        out->classic.lt    = (input.inputs[WII_CODE_CC_L] > 4095u) ? 4095u : input.inputs[WII_CODE_CC_L];
        out->classic.rt    = (input.inputs[WII_CODE_CC_R] > 4095u) ? 4095u : input.inputs[WII_CODE_CC_R];
        out->classic.l     = out->classic.lt >= CORE_WII_TRIGGER_CLICK;
        out->classic.r     = out->classic.rt >= CORE_WII_TRIGGER_CLICK;
        out->classic.zl    = p[WII_CODE_CC_ZL];
        out->classic.zr    = p[WII_CODE_CC_ZR];
        out->classic.plus  = p[WII_CODE_CC_PLUS];
        out->classic.minus = p[WII_CODE_CC_MINUS];
        out->classic.home  = p[WII_CODE_CC_HOME];
        out->classic.down  = cc_dpad[0];
        out->classic.right = cc_dpad[1];
        out->classic.left  = cc_dpad[2];
        out->classic.up    = cc_dpad[3];

        out->classic.ls_x = _core_wii_stick(&input, WII_CODE_CC_LX_LEFT, WII_CODE_CC_LX_RIGHT);
        out->classic.ls_y = _core_wii_stick(&input, WII_CODE_CC_LY_DOWN, WII_CODE_CC_LY_UP);
        out->classic.rs_x = _core_wii_stick(&input, WII_CODE_CC_RX_LEFT, WII_CODE_CC_RX_RIGHT);
        out->classic.rs_y = _core_wii_stick(&input, WII_CODE_CC_RY_DOWN, WII_CODE_CC_RY_UP);
        break;
    }
    }

    if (p[WII_CODE_SHAKE])
        _core_wii_shake(&out->accel_x, &out->accel_y, &out->accel_z);
}

void nwii_api_hook_set_rumble(bool enable)
{
    tp_evt_s evt = {
        .evt = TP_EVT_ERMRUMBLE,
        .evt_ermrumble = {.left = enable ? 255 : 0, .right = enable ? 255 : 0},
    };
    transport_evt_cb(evt);
}

void nwii_api_hook_set_leds(uint8_t led_mask)
{
    uint8_t player = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (led_mask & (1u << i))
        {
            player = i + 1;
            break;
        }
    }

    tp_evt_s evt = {
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = player},
    };
    transport_evt_cb(evt);

    tp_evt_s evt2 = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection = TP_CONNECTION_CONNECTED},
    };
    transport_evt_cb(evt2);
}

void nwii_api_hook_get_power(nwii_power_s *out)
{
    if (!out)
        return;

    out->level = 0xFF;
    out->low = false;

    if (!fuelgauge_has_driver())
        return;

    fuelgauge_status_s fstat = {0};
    fuelgauge_get_status(&fstat);
    if (!fstat.connected)
        return;

    out->level = (uint8_t)(((uint16_t)fstat.percent * 255u) / 100u);
    out->low = fstat.simple <= 1;
}

static bool _core_wii_get_generated_report(core_report_s *out)
{
    uint8_t len = 0;
    if (!nwii_api_generate_inputreport(out->data, &len))
        return false;

    out->reportformat = CORE_REPORTFORMAT_WII;
    out->size = len;
    out->reliable = false;
    return true;
}

bool core_wii_init(core_params_s *params)
{
    if (params->transport_type != GAMEPAD_TRANSPORT_BLUETOOTH)
        return false;

    // A real remote reports at ~100 Hz
    params->core_pollrate_us = 8000;

    nwii_hid_get_report_descriptor(&_wii_hid_device.hid_report_descriptor,
                                   &_wii_hid_device.hid_report_descriptor_len);
    memset(_wii_hid_device.name, 0, sizeof(_wii_hid_device.name));
    strncpy(_wii_hid_device.name, _wii_name, sizeof(_wii_hid_device.name) - 1u);

    nwii_device_config_s cfg = {.extension = NWII_EXTENSION_NUNCHUK};
    if (!nwii_api_init(&cfg))
        return false;

    _wii_mode = CORE_WII_MODE_UPRIGHT;
    mapper_set_wii_profile(WII_PROFILE_NUNCHUK);

    nwii_aim_init(&_wii_aim, NULL);
    _wii_aim_last_us = 0;

    params->hid_device = &_wii_hid_device;
    params->core_report_format    = CORE_REPORTFORMAT_WII;
    params->core_report_generator = _core_wii_get_generated_report;
    params->core_report_tunnel    = nwii_api_output_tunnel;
    // The Wii reloads its system software when a title starts, drops the link and then
    // re-initializes the remote from scratch once it reconnects
    params->core_connected        = nwii_api_connection_reset;

    if ((imu_driver_channel_count() >= 1) && (imu_config->imu_disabled != 1))
    {
        imu_set_read_mode(IMU_MODE_STANDARD);
    }

    return transport_init(params);
}

#else

bool core_wii_init(core_params_s *params)
{
    (void)params;
    return false;
}

#endif
