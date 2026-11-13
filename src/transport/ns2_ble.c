#include "transport/ns2_ble.h"

#include <stdio.h>
#include <string.h>

#include "floss/floss.h"
#include "floss/floss_gatt.h"
#include "floss/floss_hal.h"
#include "floss/floss_le.h"
#include "floss/floss_smp.h"
#include "ns2_lib_gatt.h"

#include <math.h>

#include "cores/core_ns2gc.h"
#include "devices/haptics.h"
#include "input/analog.h"
#include "input/hover.h"
#include "input/imu.h"
#include "hal/sys_hal.h"
#include "input_shared_types.h"
#include "utilities/pcm.h"

// The Switch 2 addresses a controller's attributes by handle, so the database is the retail
// controller's and nothing else: NS2-LIB-HID's table, from handle 1, FLOSS's own services cleared out
// (the table carries its own GAP and GATT services at the end).
//
// The console never runs Bluetooth pairing. Its own pairing commands, over GATT, agree a key, and it
// encrypts the link with that key from then on (the long term key request names EDIV and Rand 0).
// The library hands the key over once agreed; a paired controller advertises to its console by
// address to be taken back.

#define NS2_BLE_ATTR_MAX        64u
#define NS2_BLE_CCCD_MAX        8u
#define NS2_BLE_NO_LINK         0xFFFFu
#define NS2_BLE_ADV_INTERVAL    0x30u // 30 ms, as the retail controller
#define NS2_BLE_INPUT_INFLIGHT  2u
#define NS2_BLE_STORE_KEY       (FLOSS_STORE_KEY_APP_BASE + 0x20u)

// A trigger counts as pressed (ZL, ZR) from a quarter of its pull
#define NS2_BLE_TRIGGER_PRESSED 1024u

// Rumble, as the console writes it: for each motor a block of a header and up to three samples, a
// sample every 5 ms. The header's high nibble is 4 + the samples in the block (its low nibble counts
// samples). A sample is 40 bits, least significant byte first: frequency and amplitude for a low
// band, then for a high band, 10 bits each. None of this is published; it is read off captures of a
// Switch 2 (decaying clicks come out as decaying amplitudes). The frequency scale is the first
// Switch's with three times the steps, which puts the idle sample (385, 481) on that console's
// idle 160 Hz and 320 Hz; the amplitude scale is a guess (linear).
#define NS2_BLE_RUMBLE_BLOCK_LEN    16u
#define NS2_BLE_RUMBLE_SAMPLE_LEN   5u
#define NS2_BLE_RUMBLE_SAMPLES_MAX  3u
#define NS2_BLE_RUMBLE_COUNT_BASE   4u
#define NS2_BLE_RUMBLE_FIELD_MASK   0x3FFu
#define NS2_BLE_RUMBLE_BASE_HZ      10.0f
#define NS2_BLE_RUMBLE_OCTAVE_STEPS 96.0f
#define NS2_BLE_RUMBLE_AMP_MAX      1023.0f
// The codes reach far past what HOJA's tone generator can play: its step per sample is 16 bits and
// wraps at 1000 Hz (8 kHz sample rate), which would come out as a low tone
#define NS2_BLE_RUMBLE_MAX_HZ       990.0f

// Motion, as a Switch 2's own controllers report it: a 40-byte block of bit fields, least
// significant bit first. Not published either; read off a capture of a Joy-Con 2, where the block's
// quaternion turned out to be the integral of its own gyro fields (turns taken in the controller's
// frame) and its accelerometer fields held 4096 to the g.
//
//   bits   0-11   sample clock: counts at 800 Hz
//   bits  12-15   samples since the report before: always the step the clock took (in the capture
//                 every report's count is its clock less the clock of the report before)
//   bits  16-31   0x0E00 in all but a few reports at start
//   bits  32-36   3 in every report
//   bits  37-99   orientation: a quaternion's x, y, z, 21 bits each, 2^20 to the unit, offset by
//                 2^20; w is whatever makes it a unit quaternion, never negative. It is the
//                 orientation BEFORE this block's gyro: in the capture a report's quaternion has
//                 moved from the last one's by the last one's gyro (correlation 0.98 to 0.998 an
//                 axis), not by its own (0.94)
//   bits 100-141  accelerometer, first sample: x, y, z, 14 bits each, signed
//   bits 142-180  gyro, middle sample: 13 bits each, signed, at half scale
//   bits 181-219  accelerometer, middle sample: 13 bits each, signed, at half scale
//   bits 220-261  gyro, last sample: 14 bits each, signed, 2000 dps to 32767
//   bits 262-303  accelerometer, last sample: 14 bits each, signed
//   bits 304-319  0xFF80 most often (the low bits vary: not understood)
//
// The block's axes are a right-handed set: in the capture the accelerometer turns as the gyro says
// it should (da/dt = a x w). HOJA's own are a left-handed set, so one axis is turned round on both
// sensors. It is X: pitch is HOJA's Y and yaw its Z, and both aim the right way on a console as
// they are (with Y turned instead, aiming was smooth but upside down).
#define NS2_BLE_MOTION_LEN_AT       0x0Eu // In the input report: the block's length, then the block
#define NS2_BLE_MOTION_AT           0x0Fu
#define NS2_BLE_MOTION_LEN          0x28u
#define NS2_BLE_MOTION_CLOCK_US     1250u
#define NS2_BLE_MOTION_CLOCK_MASK   0xFFFu
#define NS2_BLE_MOTION_COUNT_MAX    15u
// Blocks further apart than this are not one motion: the link dropped, or reports stopped a while
#define NS2_BLE_MOTION_GAP_US       100000u
#define NS2_BLE_MOTION_STATUS       0x0E00u
#define NS2_BLE_MOTION_MODE         3u
#define NS2_BLE_MOTION_TAIL         0xFF80u
#define NS2_BLE_MOTION_QUAT_UNIT    1048576.0f // 2^20
#define NS2_BLE_MOTION_QUAT_MAX     2097151    // 21 bits
// HOJA's gyro counts are already in the block's unit (2000 dps to 32767), as its other modes take
// them: the gamepad's gyro sensitivity setting (1.20 by default) has been applied to the sensor's
// 70 mdps counts by the time they are read. Three full turns on a desk came to 58 to 60 mdps a
// count, where 70 / 1.20 is 58.3.
#define NS2_BLE_MOTION_RAD_PER_LSB  ((2000.0f / 32767.0f) * 0.01745329252f)

// A gyro at rest still reads a little (its zero offset), and the orientation sent is the sum of
// every reading: a gamepad never calibrated turned 35 degrees in a minute lying on a desk, and the
// console's motion then stuck and jumped. The offset is measured here whenever the gamepad lies
// still: over a stretch of blocks in which no axis moved by more than a hand could hold it to.
#define NS2_BLE_REST_BLOCKS         128u // About two thirds of a second at the console's 5 ms
#define NS2_BLE_REST_SPREAD         24   // Counts between an axis's least and most: about 1.5 dps

#define NS2_BLE_STICK_CENTER    2048
#define NS2_BLE_STICK_MIN       1
#define NS2_BLE_STICK_MAX       4095

typedef struct
{
    uint8_t host_mac[6]; // Most significant byte first
    uint8_t ltk[16];     // Most significant byte first, as the library gives it
} ns2_ble_pairing_s;

static floss_gatt_attr_t _ns2_ble_attrs[NS2_BLE_ATTR_MAX];
static floss_gatt_cccd_t _ns2_ble_cccd[NS2_BLE_CCCD_MAX];

static bool _ns2_ble_started = false;
static uint16_t _ns2_ble_link = NS2_BLE_NO_LINK;

static ns2_ble_pairing_s _ns2_ble_pairing;
static bool _ns2_ble_paired = false;
static bool _ns2_ble_pairing_new = false; // Agreed on this link: saved from the task

// A command's reply FLOSS had no room for yet
static uint8_t _ns2_ble_reply[NS2_NOTIFY_MAX_SIZE];
static uint16_t _ns2_ble_reply_handle = 0;
static uint16_t _ns2_ble_reply_len = 0;

// ---- The library's hooks ----

// Inside a command's reply: the key is taken at once (the console encrypts with it next), saved later
void ns2_api_hook_set_pairing(const ns2_pairing_s *pairing)
{
    memcpy(_ns2_ble_pairing.host_mac, pairing->host_mac, sizeof(_ns2_ble_pairing.host_mac));
    memcpy(_ns2_ble_pairing.ltk, pairing->ltk, sizeof(_ns2_ble_pairing.ltk));
    _ns2_ble_paired = true;
    _ns2_ble_pairing_new = true;
}

static uint16_t _ns2_ble_rumble_frequency(uint32_t code)
{
    float hz = NS2_BLE_RUMBLE_BASE_HZ * exp2f((float)code / NS2_BLE_RUMBLE_OCTAVE_STEPS);
    if (hz > NS2_BLE_RUMBLE_MAX_HZ)
        hz = NS2_BLE_RUMBLE_MAX_HZ;
    return pcm_frequency_to_fixedpoint_increment(hz);
}

static uint16_t _ns2_ble_rumble_amplitude(uint32_t code)
{
    return pcm_amplitude_to_fixedpoint((float)code / NS2_BLE_RUMBLE_AMP_MAX);
}

// A game's rumble. Both motors are given the left block (HOJA plays one signal; the console has
// sent the two alike so far).
void ns2_api_hook_set_haptic_raw(const uint8_t *data, uint16_t len)
{
    // The library passes the vibration attribute's write whole, report ID first, and the command
    // attribute's as the two blocks alone
    if (len > (2u * NS2_BLE_RUMBLE_BLOCK_LEN))
    {
        data++;
        len--;
    }
    if (len < NS2_BLE_RUMBLE_BLOCK_LEN)
        return;

    // A block with no samples (all zero, beside a command) changes nothing
    const uint8_t marked = data[0] >> 4;
    if (marked <= NS2_BLE_RUMBLE_COUNT_BASE)
        return;
    uint8_t count = (uint8_t)(marked - NS2_BLE_RUMBLE_COUNT_BASE);
    if (count > NS2_BLE_RUMBLE_SAMPLES_MAX)
        count = NS2_BLE_RUMBLE_SAMPLES_MAX;

    haptic_packet_s packet = {0};
    for (uint8_t i = 0; i < count; i++)
    {
        const uint8_t *sample = &data[1u + (i * NS2_BLE_RUMBLE_SAMPLE_LEN)];
        const uint64_t bits = (uint64_t)sample[0] | ((uint64_t)sample[1] << 8) | ((uint64_t)sample[2] << 16) |
                              ((uint64_t)sample[3] << 24) | ((uint64_t)sample[4] << 32);

        haptic_processed_s *pair = &packet.pairs[i];
        pair->lo_frequency_increment = _ns2_ble_rumble_frequency((uint32_t)(bits & NS2_BLE_RUMBLE_FIELD_MASK));
        pair->lo_amplitude_fixed = _ns2_ble_rumble_amplitude((uint32_t)((bits >> 10) & NS2_BLE_RUMBLE_FIELD_MASK));
        pair->hi_frequency_increment = _ns2_ble_rumble_frequency((uint32_t)((bits >> 20) & NS2_BLE_RUMBLE_FIELD_MASK));
        pair->hi_amplitude_fixed = _ns2_ble_rumble_amplitude((uint32_t)((bits >> 30) & NS2_BLE_RUMBLE_FIELD_MASK));
    }
    packet.count = count;
    haptics_set_hd(&packet);
}

// ---- Motion ----

static float _ns2_ble_quat[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z

// The gyro's zero offset, as last measured at rest, and the stretch being looked at
static int16_t _ns2_ble_gyro_zero[3] = {0, 0, 0};
// The part of a count the whole-count offset leaves behind: small, but the orientation sums it
static float _ns2_ble_gyro_zero_part[3] = {0.0f, 0.0f, 0.0f};
static int32_t _ns2_ble_rest_sum[3];
static int16_t _ns2_ble_rest_min[3];
static int16_t _ns2_ble_rest_max[3];
static uint16_t _ns2_ble_rest_count = 0;

// Takes the zero offset off this sample, and learns it when a whole stretch was still
static void _ns2_ble_gyro_rest(imu_data_s *imu)
{
    const int16_t g[3] = {imu->gx, imu->gy, imu->gz};

    for (uint8_t i = 0; i < 3u; i++)
    {
        if (!_ns2_ble_rest_count || (g[i] < _ns2_ble_rest_min[i]))
            _ns2_ble_rest_min[i] = g[i];
        if (!_ns2_ble_rest_count || (g[i] > _ns2_ble_rest_max[i]))
            _ns2_ble_rest_max[i] = g[i];
        _ns2_ble_rest_sum[i] = (_ns2_ble_rest_count ? _ns2_ble_rest_sum[i] : 0) + g[i];
    }

    if (++_ns2_ble_rest_count >= NS2_BLE_REST_BLOCKS)
    {
        bool still = true;
        for (uint8_t i = 0; i < 3u; i++)
            still = still && ((_ns2_ble_rest_max[i] - _ns2_ble_rest_min[i]) <= NS2_BLE_REST_SPREAD);
        for (uint8_t i = 0; still && (i < 3u); i++)
        {
            const float mean = (float)_ns2_ble_rest_sum[i] / (float)NS2_BLE_REST_BLOCKS;
            _ns2_ble_gyro_zero[i] = (int16_t)lroundf(mean);
            _ns2_ble_gyro_zero_part[i] = mean - (float)_ns2_ble_gyro_zero[i];
        }
        _ns2_ble_rest_count = 0;
    }

    const int32_t x = (int32_t)imu->gx - _ns2_ble_gyro_zero[0];
    const int32_t y = (int32_t)imu->gy - _ns2_ble_gyro_zero[1];
    const int32_t z = (int32_t)imu->gz - _ns2_ble_gyro_zero[2];
    imu->gx = (int16_t)((x > INT16_MAX) ? INT16_MAX : ((x < -INT16_MAX) ? -INT16_MAX : x));
    imu->gy = (int16_t)((y > INT16_MAX) ? INT16_MAX : ((y < -INT16_MAX) ? -INT16_MAX : y));
    imu->gz = (int16_t)((z > INT16_MAX) ? INT16_MAX : ((z < -INT16_MAX) ? -INT16_MAX : z));
}
static imu_data_s _ns2_ble_motion_before;
static uint64_t _ns2_ble_motion_at_us = 0;
static bool _ns2_ble_motion_running = false;

static void _ns2_ble_put_bits(uint8_t *block, uint16_t at, uint8_t width, uint32_t value)
{
    for (uint8_t i = 0; i < width; i++, at++)
    {
        if (value & (1u << i))
            block[at >> 3] |= (uint8_t)(1u << (at & 7u));
    }
}

// A signed value in a field of this many bits, held at the field's ends
static void _ns2_ble_put_signed(uint8_t *block, uint16_t at, uint8_t width, int32_t value)
{
    const int32_t max = (int32_t)(1u << (width - 1u)) - 1;
    if (value > max)
        value = max;
    if (value < -max)
        value = -max;
    _ns2_ble_put_bits(block, at, width, (uint32_t)value & ((1u << width) - 1u));
}

static void _ns2_ble_put_vector(uint8_t *block, uint16_t at, uint8_t width, int32_t x, int32_t y, int32_t z)
{
    _ns2_ble_put_signed(block, at, width, x);
    _ns2_ble_put_signed(block, (uint16_t)(at + width), width, y);
    _ns2_ble_put_signed(block, (uint16_t)(at + 2u * width), width, z);
}

// The gyro's turn over this long, taken in the controller's own frame
static void _ns2_ble_quat_turn(const imu_data_s *imu, float seconds)
{
    // X has been turned round by now (_ns2_ble_motion), so what is left of its offset adds
    const float wx = ((float)imu->gx + _ns2_ble_gyro_zero_part[0]) * NS2_BLE_MOTION_RAD_PER_LSB;
    const float wy = ((float)imu->gy - _ns2_ble_gyro_zero_part[1]) * NS2_BLE_MOTION_RAD_PER_LSB;
    const float wz = ((float)imu->gz - _ns2_ble_gyro_zero_part[2]) * NS2_BLE_MOTION_RAD_PER_LSB;
    const float rate = sqrtf(wx * wx + wy * wy + wz * wz);
    if (rate < 1e-6f)
        return;

    const float half = rate * seconds * 0.5f;
    const float s = sinf(half) / rate;
    const float d[4] = {cosf(half), wx * s, wy * s, wz * s};
    const float *q = _ns2_ble_quat;
    float r[4] = {
        q[0] * d[0] - q[1] * d[1] - q[2] * d[2] - q[3] * d[3],
        q[0] * d[1] + q[1] * d[0] + q[2] * d[3] - q[3] * d[2],
        q[0] * d[2] - q[1] * d[3] + q[2] * d[0] + q[3] * d[1],
        q[0] * d[3] + q[1] * d[2] - q[2] * d[1] + q[3] * d[0],
    };

    // A unit quaternion still, with w on the side the block can carry
    float norm = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
    if (r[0] < 0.0f)
        norm = -norm;
    for (uint8_t i = 0; i < 4u; i++)
        _ns2_ble_quat[i] = r[i] / norm;
}

static uint32_t _ns2_ble_quat_field(float component)
{
    int32_t value = (int32_t)lroundf(component * NS2_BLE_MOTION_QUAT_UNIT) + (int32_t)NS2_BLE_MOTION_QUAT_UNIT;
    if (value < 0)
        value = 0;
    if (value > NS2_BLE_MOTION_QUAT_MAX)
        value = NS2_BLE_MOTION_QUAT_MAX;
    return (uint32_t)value;
}

// The block for one input report: the sample before as its first, this one as its last
static void _ns2_ble_motion(uint8_t block[NS2_BLE_MOTION_LEN])
{
    imu_data_s imu;
    imu_access_safe(&imu);
    _ns2_ble_gyro_rest(&imu);
    imu.ax = (int16_t)-(imu.ax == INT16_MIN ? -INT16_MAX : imu.ax);
    imu.gx = (int16_t)-imu.gx;
    const uint64_t now = sys_hal_now_us();
    const uint64_t clock = now / NS2_BLE_MOTION_CLOCK_US;

    // The count is the clock's own step, so the two always agree
    uint64_t samples = clock - (_ns2_ble_motion_at_us / NS2_BLE_MOTION_CLOCK_US);
    if (samples > NS2_BLE_MOTION_COUNT_MAX)
        samples = NS2_BLE_MOTION_COUNT_MAX;

    if (!_ns2_ble_motion_running)
    {
        // Started facing wherever it is now
        _ns2_ble_quat[0] = 1.0f;
        _ns2_ble_quat[1] = 0.0f;
        _ns2_ble_quat[2] = 0.0f;
        _ns2_ble_quat[3] = 0.0f;
        _ns2_ble_motion_before = imu;
        _ns2_ble_motion_running = true;
        samples = 0;
    }
    else if ((now - _ns2_ble_motion_at_us) <= NS2_BLE_MOTION_GAP_US)
    {
        // Turned by the gyro the block before carried: this block's own is for the next
        _ns2_ble_quat_turn(&_ns2_ble_motion_before, (float)(now - _ns2_ble_motion_at_us) * 1e-6f);
    }
    _ns2_ble_motion_at_us = now;

    const imu_data_s *before = &_ns2_ble_motion_before;
    const int32_t gx = imu.gx;
    const int32_t gy = imu.gy;
    const int32_t gz = imu.gz;

    memset(block, 0, NS2_BLE_MOTION_LEN);
    _ns2_ble_put_bits(block, 0, 12, (uint32_t)(clock & NS2_BLE_MOTION_CLOCK_MASK));
    _ns2_ble_put_bits(block, 12, 4, (uint32_t)samples);
    _ns2_ble_put_bits(block, 16, 16, NS2_BLE_MOTION_STATUS);
    _ns2_ble_put_bits(block, 32, 5, NS2_BLE_MOTION_MODE);
    _ns2_ble_put_bits(block, 37, 21, _ns2_ble_quat_field(_ns2_ble_quat[1]));
    _ns2_ble_put_bits(block, 58, 21, _ns2_ble_quat_field(_ns2_ble_quat[2]));
    _ns2_ble_put_bits(block, 79, 21, _ns2_ble_quat_field(_ns2_ble_quat[3]));
    _ns2_ble_put_vector(block, 100, 14, before->ax, before->ay, before->az);
    _ns2_ble_put_vector(block, 142, 13, ((int32_t)before->gx + gx) / 4, ((int32_t)before->gy + gy) / 4,
                        ((int32_t)before->gz + gz) / 4);
    _ns2_ble_put_vector(block, 181, 13, ((int32_t)before->ax + imu.ax) / 4, ((int32_t)before->ay + imu.ay) / 4,
                        ((int32_t)before->az + imu.az) / 4);
    _ns2_ble_put_vector(block, 220, 14, gx, gy, gz);
    _ns2_ble_put_vector(block, 262, 14, imu.ax, imu.ay, imu.az);
    _ns2_ble_put_bits(block, 304, 16, NS2_BLE_MOTION_TAIL);

    _ns2_ble_motion_before = imu;
}

static uint16_t _ns2_ble_stick(int value)
{
    value += NS2_BLE_STICK_CENTER;
    if (value < NS2_BLE_STICK_MIN)
        value = NS2_BLE_STICK_MIN;
    if (value > NS2_BLE_STICK_MAX)
        value = NS2_BLE_STICK_MAX;
    return (uint16_t)value;
}

// Each button as its label reads, whatever profile the gamepad's own mode has
void ns2_ble_input(ns2_input_s *out)
{
    mapper_input_s in;
    analog_data_s sticks;
    hover_access_safe(&in);
    analog_access_safe(&sticks, ANALOG_ACCESS_DEADZONE_DATA);

    memset(out, 0, sizeof(*out));
    out->a = in.presses[INPUT_CODE_SOUTH];
    out->b = in.presses[INPUT_CODE_EAST];
    out->x = in.presses[INPUT_CODE_WEST];
    out->y = in.presses[INPUT_CODE_NORTH];
    out->up = in.presses[INPUT_CODE_UP];
    out->down = in.presses[INPUT_CODE_DOWN];
    out->left = in.presses[INPUT_CODE_LEFT];
    out->right = in.presses[INPUT_CODE_RIGHT];
    out->l = in.presses[INPUT_CODE_LB];
    out->r = in.presses[INPUT_CODE_RB];
    // A trigger with no click of its own is pressed by its pull
    out->zl = in.presses[INPUT_CODE_LT] || (in.inputs[INPUT_CODE_LT_ANALOG] >= NS2_BLE_TRIGGER_PRESSED);
    out->zr = in.presses[INPUT_CODE_RT] || (in.inputs[INPUT_CODE_RT_ANALOG] >= NS2_BLE_TRIGGER_PRESSED);
    out->plus = in.presses[INPUT_CODE_START];
    out->minus = in.presses[INPUT_CODE_SELECT];
    out->home = in.presses[INPUT_CODE_HOME];
    out->capture = in.presses[INPUT_CODE_SHARE];
    out->l_js = in.presses[INPUT_CODE_LS];
    out->r_js = in.presses[INPUT_CODE_RS];
    out->gl = in.presses[INPUT_CODE_LP1];
    out->gr = in.presses[INPUT_CODE_RP1];

    out->ls_x = _ns2_ble_stick(sticks.lx);
    out->ls_y = _ns2_ble_stick(sticks.ly);
    out->rs_x = _ns2_ble_stick(sticks.rx);
    out->rs_y = _ns2_ble_stick(sticks.ry);

    // Pulled through to the click reads full, as on the retail controller
    const uint16_t lt = in.inputs[INPUT_CODE_LT_ANALOG] >> 4;
    const uint16_t rt = in.inputs[INPUT_CODE_RT_ANALOG] >> 4;
    out->lt = (out->zl || (lt > 255u)) ? 255u : (uint8_t)lt;
    out->rt = (out->zr || (rt > 255u)) ? 255u : (uint8_t)rt;
}

bool ns2_ble_running(void)
{
    return _ns2_ble_started;
}

bool ns2_ble_linked(void)
{
    return _ns2_ble_link != NS2_BLE_NO_LINK;
}

// ---- GATT ----

static int _ns2_ble_read(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr, uint16_t offset,
                         uint8_t *out, uint16_t cap)
{
    (void)conn;
    (void)attr;
    return ns2_api_ble_read(attr_handle, offset, out, cap);
}

static uint8_t _ns2_ble_write(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr, uint16_t offset,
                              const uint8_t *data, uint16_t len, uint8_t flags)
{
    (void)conn;
    (void)attr;
    (void)offset;
    (void)flags;
    ns2_api_ble_write(attr_handle, data, len);
    return 0;
}

static bool _ns2_ble_ltk(uint16_t handle, uint16_t ediv, const uint8_t rand[8], uint8_t ltk[16], void *user)
{
    (void)handle;
    (void)ediv;
    (void)rand;
    (void)user;
    if (!_ns2_ble_paired)
        return false;
    // FLOSS takes it least significant byte first
    for (uint8_t i = 0; i < sizeof(_ns2_ble_pairing.ltk); i++)
        ltk[i] = _ns2_ble_pairing.ltk[sizeof(_ns2_ble_pairing.ltk) - 1u - i];
    return true;
}

// The library's table as FLOSS attributes: how many, or 0 when the handles would not come out as
// the table's own. Both number from 1, one attribute to a handle.
static uint16_t _ns2_ble_build_gatt(void)
{
    uint16_t count = 0;
    const ns2_gatt_attribute_s *table = ns2_gatt_get_table(&count);
    uint16_t n = 0;
    uint8_t cccds = 0;

    for (uint16_t i = 0; i < count; i++)
    {
        const ns2_gatt_attribute_s *entry = &table[i];
        if ((entry->handle != (n + 1u)) || ((n + 2u) > NS2_BLE_ATTR_MAX))
            return 0;

        floss_gatt_attr_t *attr = &_ns2_ble_attrs[n];
        memset(attr, 0, sizeof(*attr));

        switch (entry->kind)
        {
            case NS2_ATT_PRIMARY_SERVICE:
                attr->type = FLOSS_GATT_UUID_PRIMARY_SERVICE;
                attr->perms = FLOSS_GATT_P_READ;
                if (entry->uuid128)
                {
                    attr->value = (void *)entry->uuid128;
                    attr->max_len = 16;
                }
                else
                {
                    attr->aux = entry->uuid16;
                }
                n++;
                break;

            case NS2_ATT_CHARACTERISTIC:
            {
                attr->type = FLOSS_GATT_UUID_CHARACTERISTIC;
                attr->perms = FLOSS_GATT_P_READ;
                attr->aux = entry->properties;

                floss_gatt_attr_t *value = &_ns2_ble_attrs[n + 1u];
                memset(value, 0, sizeof(*value));
                value->type = entry->uuid16;
                value->uuid128 = entry->uuid128;
                if (entry->value)
                {
                    value->perms = FLOSS_GATT_P_READ;
                    value->value = (void *)entry->value;
                    value->max_len = entry->value_len;
                }
                else
                {
                    value->perms = FLOSS_GATT_P_RW;
                    value->read = _ns2_ble_read;
                    value->write = _ns2_ble_write;
                }
                n += 2u;
                break;
            }

            case NS2_ATT_CCC:
                if (cccds >= NS2_BLE_CCCD_MAX)
                    return 0;
                attr->type = FLOSS_GATT_UUID_CCCD;
                attr->perms = FLOSS_GATT_P_RW;
                attr->value = &_ns2_ble_cccd[cccds++];
                attr->max_len = 2;
                attr->write = _ns2_ble_write;
                n++;
                break;

            case NS2_ATT_DESCRIPTOR:
            default:
                attr->uuid128 = entry->uuid128;
                attr->type = entry->uuid16;
                attr->perms = FLOSS_GATT_P_RW;
                attr->read = _ns2_ble_read;
                attr->write = _ns2_ble_write;
                n++;
                break;
        }
    }
    return n;
}

// ---- The link ----

// To the paired console by address, asking to be taken back; to any console otherwise
static void _ns2_ble_advertise(void)
{
    uint8_t adv[NS2_ADV_DATA_MAX_SIZE];
    const uint8_t len = ns2_api_get_advertisement(adv, _ns2_ble_paired);

    floss_le_adv_params_t params;
    floss_le_adv_params_defaults(&params);
    params.interval_min = NS2_BLE_ADV_INTERVAL;
    params.interval_max = NS2_BLE_ADV_INTERVAL;
    params.own_addr_type = FLOSS_LE_ADDR_PUBLIC;
    // Restarted here, with the advert for the pairing as it then stands
    params.auto_restart = false;
    floss_le_adv_set_params(&params);
    floss_le_adv_set_data(adv, len);
    floss_le_adv_start();
}

static void _ns2_ble_event(const floss_le_evt_t *evt, void *user)
{
    (void)user;

    switch (evt->type)
    {
        case FLOSS_LE_EVT_CONNECTED:
        {
            uint8_t host_mac[6];
            for (uint8_t i = 0; i < sizeof(host_mac); i++)
                host_mac[i] = evt->addr.b[sizeof(host_mac) - 1u - i];
            _ns2_ble_link = evt->handle;
            _ns2_ble_reply_len = 0;
            _ns2_ble_motion_running = false;
            ns2_api_ble_connected(host_mac);
            break;
        }

        case FLOSS_LE_EVT_DISCONNECTED:
            if (evt->handle != _ns2_ble_link)
                break;
            _ns2_ble_link = NS2_BLE_NO_LINK;
            ns2_api_ble_disconnected();
            _ns2_ble_advertise();
            break;

        default:
            break;
    }
}

bool ns2_ble_start(const uint8_t dev_mac[6])
{
    if (_ns2_ble_started)
        return true;

    _ns2_ble_paired = floss_hal_store_read(NS2_BLE_STORE_KEY, &_ns2_ble_pairing, sizeof(_ns2_ble_pairing)) ==
                      (int)sizeof(_ns2_ble_pairing);

    ns2_device_config_s cfg = {0};
    cfg.type = NS2_DEVTYPE_PRO2;
    cfg.transport = NS2_TRANSPORT_BLE;
    memcpy(cfg.device_mac, dev_mac, sizeof(cfg.device_mac));
    if (_ns2_ble_paired)
        memcpy(cfg.host_mac, _ns2_ble_pairing.host_mac, sizeof(cfg.host_mac));
    // A serial in the retail form (four letters, ten digits), the digits from this controller's address
    const uint32_t id = ((uint32_t)dev_mac[2] << 24) | ((uint32_t)dev_mac[3] << 16) |
                        ((uint32_t)dev_mac[4] << 8) | dev_mac[5];
    snprintf((char *)cfg.device_serial, sizeof(cfg.device_serial), "HJW5%010lu", (unsigned long)id);
    memset(&cfg.colors, 0xFF, sizeof(cfg.colors));

    if (ns2_api_init(&cfg) != NS2_CONFIG_OK)
        return false;
    if (_ns2_ble_paired)
        ns2_api_set_ltk(_ns2_ble_pairing.ltk);

    floss_gatt_clear();
    memset(_ns2_ble_cccd, 0, sizeof(_ns2_ble_cccd));
    const uint16_t attrs = _ns2_ble_build_gatt();
    if (!attrs || (floss_gatt_register(_ns2_ble_attrs, attrs) != 1u))
        return false;
    floss_smp_set_ltk_provider(_ns2_ble_ltk, NULL);

    // The console sets the link up as it likes (interval, PHY): it is asked for nothing but the
    // 2M PHY, which it moves a retail controller to anyway
    floss_le_link_policy_t policy;
    floss_le_link_policy_defaults(&policy);
    policy.update_delay_us = 0;
    floss_le_set_link_policy(&policy);

    floss_le_add_listener(_ns2_ble_event, NULL);
    _ns2_ble_started = true;
    _ns2_ble_advertise();
    return true;
}

void ns2_ble_task(void)
{
    if (!_ns2_ble_started)
        return;

    // The console's vibration samples run to their end here: no core's task is stepping them
    core_ns2gc_rumble_task();

    if (_ns2_ble_pairing_new)
    {
        _ns2_ble_pairing_new = false;
        floss_hal_store_write(NS2_BLE_STORE_KEY, &_ns2_ble_pairing, sizeof(_ns2_ble_pairing));
    }

    if (_ns2_ble_link == NS2_BLE_NO_LINK)
        return;

    // Replies to the console's commands first, in order: one FLOSS has no room for waits here
    for (;;)
    {
        if (!_ns2_ble_reply_len &&
            !ns2_api_ble_get_notification(&_ns2_ble_reply_handle, _ns2_ble_reply, &_ns2_ble_reply_len))
            break;
        const floss_err_t err =
            floss_gatt_notify(_ns2_ble_link, _ns2_ble_reply_handle, _ns2_ble_reply, _ns2_ble_reply_len);
        if ((err == FLOSS_ERR_BUSY) || (err == FLOSS_ERR_NOMEM))
            return;
        _ns2_ble_reply_len = 0;
    }

    // One fresh input report to a connection event, paced by the link itself: a report is made
    // only once the controller holds fewer than two (one on the air, one waiting its turn). A
    // timer beside the link's own clock drifts against it, and a report made early waits and
    // arrives stale. The library has no report until the console asks for them.
    if (!floss_hci_acl_can_send_fresh(_ns2_ble_link, NS2_BLE_INPUT_INFLIGHT))
        return;

    uint8_t report[NS2_INPUT_REPORT_SIZE];
    if (!ns2_api_generate_inputreport(report))
        return;

    // The library leaves the motion block empty once the console has turned motion on: filled here
    if (report[NS2_BLE_MOTION_LEN_AT] == NS2_BLE_MOTION_LEN)
        _ns2_ble_motion(&report[NS2_BLE_MOTION_AT]);
    else
        _ns2_ble_motion_running = false;

    floss_gatt_notify(_ns2_ble_link, NS2_HANDLE_PRO2_INPUT_VALUE, report, sizeof(report));
}
