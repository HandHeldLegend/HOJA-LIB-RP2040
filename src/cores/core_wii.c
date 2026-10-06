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

// Gyro aim: degrees of rotation from the centre to the screen edge
#define CORE_WII_AIM_YAW_RANGE_DEG      12.5f
#define CORE_WII_AIM_PITCH_RANGE_DEG    10.0f

// HOJA's IMU frame is +X left, +Y forward, +Z up (NS-LIB swaps X/Y into the Switch's frame).
// Yaw is rotation about Z and pitch about X; these signs make right / up positive.
#define CORE_WII_AIM_YAW_SIGN           (-1.0f)
#define CORE_WII_AIM_PITCH_SIGN         (-1.0f)

// Gyro bias tracking: once both aim axes have stayed under STILL_DPS for STILL_US, the residual
// offset is learned with time constant BIAS_TAU_S. DEADBAND_DPS hides what is left.
#define CORE_WII_AIM_STILL_DPS          2.0f
#define CORE_WII_AIM_STILL_US           500000
#define CORE_WII_AIM_BIAS_TAU_S         1.0f
#define CORE_WII_AIM_DEADBAND_DPS       0.75f

// Right stick nudges the pointer in Nunchuk mode: screen-halves per second at full tilt
#define CORE_WII_AIM_STICK_SPEED        1.5f

// Shake: square wave on the accelerometer while the shake button is held
#define CORE_WII_SHAKE_MG               3000
#define CORE_WII_SHAKE_HALF_PERIOD_US   32000

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

typedef struct
{
    float    x;
    float    y;
    float    bias_yaw_dps;
    float    bias_pitch_dps;
    uint32_t still_us;
    uint64_t last_us;
} core_wii_aim_s;

static core_wii_aim_s _wii_aim = {0};

static inline float _core_wii_clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static inline int16_t _core_wii_mg(int16_t raw)
{
    float mg = (float)raw * CORE_WII_ACCEL_MG_PER_LSB;
    return (int16_t)_core_wii_clampf(mg, -32000.0f, 32000.0f);
}

static inline float _core_wii_stick_norm(uint16_t axis)
{
    return ((float)axis - 2048.0f) / 2048.0f;
}

static inline float _core_wii_absf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static inline float _core_wii_deadband(float v, float band)
{
    if (_core_wii_absf(v) <= band)
        return 0.0f;
    return (v > 0.0f) ? (v - band) : (v + band);
}

// Learn the gyro's resting offset while the controller is held still, so the pointer does not
// creep. Deliberate slow aiming never stays under the threshold long enough to be absorbed.
static void _core_wii_aim_track_bias(float yaw_dps, float pitch_dps, float dt)
{
    const bool still = (_core_wii_absf(yaw_dps - _wii_aim.bias_yaw_dps) < CORE_WII_AIM_STILL_DPS) &&
                       (_core_wii_absf(pitch_dps - _wii_aim.bias_pitch_dps) < CORE_WII_AIM_STILL_DPS);

    if (!still)
    {
        _wii_aim.still_us = 0;
        return;
    }

    if (_wii_aim.still_us < CORE_WII_AIM_STILL_US)
    {
        _wii_aim.still_us += (uint32_t)(dt * 1000000.0f);
        return;
    }

    const float k = _core_wii_clampf(dt / CORE_WII_AIM_BIAS_TAU_S, 0.0f, 1.0f);
    _wii_aim.bias_yaw_dps   += (yaw_dps - _wii_aim.bias_yaw_dps) * k;
    _wii_aim.bias_pitch_dps += (pitch_dps - _wii_aim.bias_pitch_dps) * k;
}

// Integrate the gyro into a pointer position. The edges absorb further rotation, so turning
// back from an edge recentres the cursor naturally.
static void _core_wii_aim_update(const imu_data_s *imu, float stick_x, float stick_y, bool recenter)
{
    const uint64_t now = sys_hal_now_us();
    float dt = (_wii_aim.last_us == 0) ? 0.0f : (float)(now - _wii_aim.last_us) / 1000000.0f;
    _wii_aim.last_us = now;

    if (dt > 0.05f) dt = 0.05f;

    if (recenter)
    {
        _wii_aim.x = 0.0f;
        _wii_aim.y = 0.0f;
        return;
    }

    const float raw_yaw_dps   = (float)imu->gz * CORE_WII_GYRO_DPS_PER_LSB * CORE_WII_AIM_YAW_SIGN;
    const float raw_pitch_dps = (float)imu->gx * CORE_WII_GYRO_DPS_PER_LSB * CORE_WII_AIM_PITCH_SIGN;

    _core_wii_aim_track_bias(raw_yaw_dps, raw_pitch_dps, dt);

    const float yaw_dps   = _core_wii_deadband(raw_yaw_dps - _wii_aim.bias_yaw_dps, CORE_WII_AIM_DEADBAND_DPS);
    const float pitch_dps = _core_wii_deadband(raw_pitch_dps - _wii_aim.bias_pitch_dps, CORE_WII_AIM_DEADBAND_DPS);

    _wii_aim.x += (yaw_dps * dt) / CORE_WII_AIM_YAW_RANGE_DEG;
    _wii_aim.y += (pitch_dps * dt) / CORE_WII_AIM_PITCH_RANGE_DEG;

    _wii_aim.x += stick_x * CORE_WII_AIM_STICK_SPEED * dt;
    _wii_aim.y += stick_y * CORE_WII_AIM_STICK_SPEED * dt;

    _wii_aim.x = _core_wii_clampf(_wii_aim.x, -1.0f, 1.0f);
    _wii_aim.y = _core_wii_clampf(_wii_aim.y, -1.0f, 1.0f);
}

static bool _core_wii_code_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
        return false;

    return input->presses[code] || (input->inputs[code] > 0);
}

// The board's power button is whatever its ship-mode macro holds. A short tap swaps between
// the Nunchuk and the Classic Controller Pro.
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
            const bool to_classic = (nwii_api_get_extension() == NWII_EXTENSION_NUNCHUK);
            nwii_api_set_extension(to_classic ? NWII_EXTENSION_CLASSIC_PRO : NWII_EXTENSION_NUNCHUK);
            rgb_send_notification(to_classic ? COLOR_BLUE : COLOR_WHITE);
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

void nwii_api_hook_get_input(nwii_input_s *out)
{
    if (!out)
        return;

    _core_wii_power_tap_task();

    mapper_input_s input = mapper_get_input();

    bool dpad[4] = {input.presses[SWITCH_CODE_DOWN], input.presses[SWITCH_CODE_RIGHT],
                    input.presses[SWITCH_CODE_LEFT], input.presses[SWITCH_CODE_UP]};
    dpad_translate_input(dpad);

    const uint16_t lx = (uint16_t)_core_wii_clampf((float)mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LX_LEFT], input.inputs[SWITCH_CODE_LX_RIGHT]), 0, 4095);
    const uint16_t ly = (uint16_t)_core_wii_clampf((float)mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LY_DOWN], input.inputs[SWITCH_CODE_LY_UP]), 0, 4095);
    const uint16_t rx = (uint16_t)_core_wii_clampf((float)mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RX_LEFT], input.inputs[SWITCH_CODE_RX_RIGHT]), 0, 4095);
    const uint16_t ry = (uint16_t)_core_wii_clampf((float)mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RY_DOWN], input.inputs[SWITCH_CODE_RY_UP]), 0, 4095);

    const bool nunchuk_mode = (nwii_api_get_extension() == NWII_EXTENSION_NUNCHUK);

    imu_data_s imu = {0};
    imu_access_safe(&imu);

    // The pointer follows the gyro in both modes so the Wii Menu stays usable. The right stick
    // only nudges it in Nunchuk mode, where it has no other job.
    _core_wii_aim_update(&imu,
                         nunchuk_mode ? _core_wii_stick_norm(rx) : 0.0f,
                         nunchuk_mode ? _core_wii_stick_norm(ry) : 0.0f,
                         input.presses[SWITCH_CODE_RS]);
    nwii_ir_set_pointer(out->ir, _wii_aim.x, _wii_aim.y);

    if (nunchuk_mode)
    {
        // The gamepad is the Wii Remote. HOJA's IMU frame (+X left, +Y forward, +Z up) already
        // lines up with the remote's.
        out->accel_x = _core_wii_mg(imu.ax);
        out->accel_y = _core_wii_mg(imu.ay);
        out->accel_z = _core_wii_mg(imu.az);

        if (input.presses[SWITCH_CODE_R])
            _core_wii_shake(&out->accel_x, &out->accel_y, &out->accel_z);

        out->remote.a     = input.presses[SWITCH_CODE_A];
        out->remote.b     = input.presses[SWITCH_CODE_B] || input.presses[SWITCH_CODE_ZR];
        out->remote.one   = input.presses[SWITCH_CODE_Y];
        out->remote.two   = input.presses[SWITCH_CODE_X];
        out->remote.plus  = input.presses[SWITCH_CODE_PLUS];
        out->remote.minus = input.presses[SWITCH_CODE_MINUS];
        out->remote.home  = input.presses[SWITCH_CODE_HOME];
        out->remote.down  = dpad[0];
        out->remote.right = dpad[1];
        out->remote.left  = dpad[2];
        out->remote.up    = dpad[3];

        out->nunchuk.c = input.presses[SWITCH_CODE_L];
        out->nunchuk.z = input.presses[SWITCH_CODE_ZL];
        out->nunchuk.stick_x = lx;
        out->nunchuk.stick_y = ly;

        if (input.presses[SWITCH_CODE_LS])
            _core_wii_shake(&out->nunchuk.accel_x, &out->nunchuk.accel_y, &out->nunchuk.accel_z);
    }
    else
    {
        out->classic.a     = input.presses[SWITCH_CODE_A];
        out->classic.b     = input.presses[SWITCH_CODE_B];
        out->classic.x     = input.presses[SWITCH_CODE_X];
        out->classic.y     = input.presses[SWITCH_CODE_Y];
        out->classic.l     = input.presses[SWITCH_CODE_L];
        out->classic.r     = input.presses[SWITCH_CODE_R];
        out->classic.zl    = input.presses[SWITCH_CODE_ZL];
        out->classic.zr    = input.presses[SWITCH_CODE_ZR];
        out->classic.plus  = input.presses[SWITCH_CODE_PLUS];
        out->classic.minus = input.presses[SWITCH_CODE_MINUS];
        out->classic.home  = input.presses[SWITCH_CODE_HOME];
        out->classic.down  = dpad[0];
        out->classic.right = dpad[1];
        out->classic.left  = dpad[2];
        out->classic.up    = dpad[3];

        out->classic.ls_x = lx;
        out->classic.ls_y = ly;
        out->classic.rs_x = rx;
        out->classic.rs_y = ry;
    }
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

    memset(&_wii_aim, 0, sizeof(_wii_aim));

    params->hid_device = &_wii_hid_device;
    params->core_report_format    = CORE_REPORTFORMAT_WII;
    params->core_report_generator = _core_wii_get_generated_report;
    params->core_report_tunnel    = nwii_api_output_tunnel;

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
