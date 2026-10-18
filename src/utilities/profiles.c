#include "utilities/profiles.h"

#include <stddef.h>
#include <string.h>

#include "usb/webusb.h"

#if defined(HOJA_BOOTLOADER)

#include "devices/animations/anm_authentic.h"
#include "devices/haptics.h"
#include "devices/rgb.h"
#include "hal/rp2040/record_store.h"
#include "input/analog.h"
#include "input/mapper.h"
#include "utilities/settings.h"

#define PROFILES_RECORD_GLOBAL 0u
#define PROFILES_RECORD_FIRST  1u // Profile n is record 1 + n
#define PROFILES_OPS_LEN       4u

// The profile's parts of the blocks, each one stretch of bytes: from the left stick's angle map to the
// right stick's curve, from motion on/off to the per-mode mask, haptic strength and trigger rumble
#define PROFILES_ANALOG_AT  offsetof(analogConfig_s, joy_config_l)
#define PROFILES_ANALOG_LEN (offsetof(analogConfig_s, r_exp_scaler) + 1u - PROFILES_ANALOG_AT)
#define PROFILES_IMU_AT     offsetof(imuConfig_s, imu_disabled)
#define PROFILES_IMU_LEN    (offsetof(imuConfig_s, imu_mode_disable_mask) + 2u - PROFILES_IMU_AT)
#define PROFILES_HAPTIC_AT  offsetof(hapticConfig_s, haptic_strength)
#define PROFILES_HAPTIC_LEN (offsetof(hapticConfig_s, haptic_triggers) + 1u - PROFILES_HAPTIC_AT)

typedef struct __attribute__((packed))
{
    uint8_t active;
    uint8_t reserved[3];
} profiles_global_head_s; // The global record: this, then all of live_settings

typedef struct __attribute__((packed))
{
    uint8_t name[PROFILES_NAME_LEN];
    uint8_t analog[PROFILES_ANALOG_LEN];
    uint8_t imu[PROFILES_IMU_LEN];
    uint8_t haptic[PROFILES_HAPTIC_LEN];
    uint8_t rgb[RGB_CFB_SIZE];
    uint8_t input[INPUT_CFB_SIZE];
} profile_s;

_Static_assert(sizeof(profile_s) <= RECORD_STORE_PAYLOAD_MAX, "a profile fills more than its record");
_Static_assert(sizeof(profiles_global_head_s) + sizeof(settings_live_s) <= RECORD_STORE_PAYLOAD_MAX,
               "the global settings fill more than their record");

typedef struct
{
    profiles_op_t op;
    uint8_t profile;
    uint8_t from;
} profiles_pending_s;

static bool _profiles_ready = false;
static uint8_t _profiles_active = 0;
static uint8_t _profiles_names[PROFILES_COUNT][PROFILES_NAME_LEN];

// Records waiting for the task, written one a pass
static bool _profiles_global_dirty = false;
static bool _profiles_active_dirty = false;
static uint8_t _profiles_create = 0; // Profiles to make from the live settings (bit per profile)
static profile_s _profiles_outgoing; // The profile switched away from, as it was
static int8_t _profiles_outgoing_at = -1;
static profiles_pending_s _profiles_ops[PROFILES_OPS_LEN]; // Renames and copies of profiles not active
static uint8_t _profiles_ops_count = 0;

static profile_s _profiles_build;

// --- The live settings and a profile ---

static void _profiles_capture(profile_s *p, uint8_t profile)
{
    memcpy(p->name, _profiles_names[profile], PROFILES_NAME_LEN);
    memcpy(p->analog, &live_settings.analog_configuration_block[PROFILES_ANALOG_AT], PROFILES_ANALOG_LEN);
    memcpy(p->imu, &live_settings.imu_configuration_block[PROFILES_IMU_AT], PROFILES_IMU_LEN);
    memcpy(p->haptic, &live_settings.haptic_configuration_block[PROFILES_HAPTIC_AT], PROFILES_HAPTIC_LEN);
    memcpy(p->rgb, live_settings.rgb_configuration_block, RGB_CFB_SIZE);
    memcpy(p->input, live_settings.input_configuration_block, INPUT_CFB_SIZE);
}

// Into live_settings; running, the parts that hold state take it up as a block write would
static void _profiles_apply(const profile_s *p, bool running)
{
    memcpy(&live_settings.analog_configuration_block[PROFILES_ANALOG_AT], p->analog, PROFILES_ANALOG_LEN);
    memcpy(&live_settings.imu_configuration_block[PROFILES_IMU_AT], p->imu, PROFILES_IMU_LEN);
    memcpy(&live_settings.haptic_configuration_block[PROFILES_HAPTIC_AT], p->haptic, PROFILES_HAPTIC_LEN);
    memcpy(live_settings.rgb_configuration_block, p->rgb, RGB_CFB_SIZE);
    memcpy(live_settings.input_configuration_block, p->input, INPUT_CFB_SIZE);

    if (!running)
        return;
    analog_init();
    rgb_init(-1, -1);
    haptics_init();
    mapper_init();
    anm_authentic_refresh();
}

static const profile_s *_profiles_stored(uint8_t profile)
{
    uint32_t len = 0;
    const uint8_t *p = record_store_find((uint16_t)(PROFILES_RECORD_FIRST + profile), &len);
    return (p && (len == sizeof(profile_s))) ? (const profile_s *)p : NULL;
}

// --- Public ---

bool profiles_init(void)
{
    record_store_init();

    uint32_t len = 0;
    const uint8_t *global = record_store_find(PROFILES_RECORD_GLOBAL, &len);
    if (global && (len == sizeof(profiles_global_head_s) + sizeof(settings_live_s)))
    {
        const profiles_global_head_s *head = (const profiles_global_head_s *)global;
        _profiles_active = (head->active < PROFILES_COUNT) ? head->active : 0;
        memcpy(&live_settings, &global[sizeof(profiles_global_head_s)], sizeof(settings_live_s));
    }
    else
    {
        // The first time: the settings page as it was becomes the global record and every profile
        _profiles_active = 0;
        _profiles_global_dirty = true;
    }

    // A profile without a record (the first time, or a power cut before it was written) starts as a
    // copy of the active one, unnamed
    memset(_profiles_names, 0, sizeof(_profiles_names));
    for (uint8_t i = 0; i < PROFILES_COUNT; i++)
    {
        const profile_s *p = _profiles_stored(i);
        if (p)
            memcpy(_profiles_names[i], p->name, PROFILES_NAME_LEN);
        else
            _profiles_create |= (uint8_t)(1u << i);
    }

    const profile_s *active = _profiles_stored(_profiles_active);
    if (active)
        _profiles_apply(active, false);

    _profiles_ready = true;
    return true;
}

bool profiles_save(void)
{
    if (!_profiles_ready)
        return false;
    _profiles_global_dirty = true;
    _profiles_active_dirty = true;
    return true;
}

static void _profiles_run_op(const profiles_pending_s *op)
{
    if (op->op == PROFILES_OP_COPY)
    {
        if (op->from == _profiles_active)
            _profiles_capture(&_profiles_build, op->from);
        else if (_profiles_stored(op->from))
            memcpy(&_profiles_build, _profiles_stored(op->from), sizeof(profile_s));
        else
            return;
    }
    else if (_profiles_stored(op->profile))
    {
        memcpy(&_profiles_build, _profiles_stored(op->profile), sizeof(profile_s));
    }
    else
    {
        return;
    }

    memcpy(_profiles_build.name, _profiles_names[op->profile], PROFILES_NAME_LEN);
    record_store_write((uint16_t)(PROFILES_RECORD_FIRST + op->profile), &_profiles_build, sizeof(profile_s),
                       NULL, 0);
}

static bool _profiles_pending(void)
{
    return (_profiles_outgoing_at >= 0) || _profiles_create || _profiles_active_dirty || _profiles_global_dirty ||
           _profiles_ops_count;
}

// Writes the next waiting record. Each holds the other core for a few tens of ms. False when it
// couldn't (it waits for the next try).
static bool _profiles_write_one(void)
{
    if (_profiles_outgoing_at >= 0)
    {
        if (!record_store_write((uint16_t)(PROFILES_RECORD_FIRST + _profiles_outgoing_at), &_profiles_outgoing,
                                sizeof(profile_s), NULL, 0))
            return false;
        _profiles_outgoing_at = -1;
        return true;
    }

    if (_profiles_create)
    {
        uint8_t i = 0;
        while (!(_profiles_create & (1u << i)))
            i++;
        _profiles_capture(&_profiles_build, i);
        if (!record_store_write((uint16_t)(PROFILES_RECORD_FIRST + i), &_profiles_build, sizeof(profile_s), NULL, 0))
            return false;
        _profiles_create &= (uint8_t)~(1u << i);
        return true;
    }

    if (_profiles_active_dirty)
    {
        _profiles_capture(&_profiles_build, _profiles_active);
        if (!record_store_write((uint16_t)(PROFILES_RECORD_FIRST + _profiles_active), &_profiles_build,
                                sizeof(profile_s), NULL, 0))
            return false;
        _profiles_active_dirty = false;
        return true;
    }

    if (_profiles_global_dirty)
    {
        profiles_global_head_s head = {.active = _profiles_active, .reserved = {0}};
        if (!record_store_write(PROFILES_RECORD_GLOBAL, &head, sizeof(head), &live_settings, sizeof(settings_live_s)))
            return false;
        _profiles_global_dirty = false;
        return true;
    }

    if (_profiles_ops_count)
    {
        _profiles_run_op(&_profiles_ops[0]);
        _profiles_ops_count--;
        memmove(&_profiles_ops[0], &_profiles_ops[1], _profiles_ops_count * sizeof(profiles_pending_s));
        return true;
    }
    return false;
}

// Everything waiting, written now: the app's profile changes are answered once they are stored. The
// task alone isn't enough there: while the app talks over USB the scheduler can go seconds without
// picking it.
static bool _profiles_flush(void)
{
    for (uint8_t tries = 0; _profiles_pending() && (tries < 2u * (PROFILES_COUNT + PROFILES_OPS_LEN)); tries++)
        _profiles_write_one();
    return !_profiles_pending();
}

void profiles_task(uint64_t now_us)
{
    (void)now_us;
    if (_profiles_ready)
        _profiles_write_one();
}

static bool _profiles_queue(profiles_op_t op, uint8_t profile, uint8_t from)
{
    if (_profiles_ops_count >= PROFILES_OPS_LEN)
        return false;
    _profiles_ops[_profiles_ops_count++] = (profiles_pending_s){.op = op, .profile = profile, .from = from};
    return true;
}

// The one switched away from is saved as it was; the next one takes over live. What was waiting
// goes first, so the next one is read as stored.
static bool _profiles_switch(uint8_t profile)
{
    if (profile == _profiles_active)
        return true;
    if (!_profiles_flush() || !_profiles_stored(profile))
        return false;

    _profiles_capture(&_profiles_outgoing, _profiles_active);
    _profiles_outgoing_at = (int8_t)_profiles_active;
    _profiles_apply(_profiles_stored(profile), true);
    _profiles_active = profile;
    _profiles_active_dirty = false;
    _profiles_global_dirty = true;
    return true;
}

static bool _profiles_copy(uint8_t profile, uint8_t from)
{
    if ((profile == from) || !_profiles_flush())
        return false;

    if (profile != _profiles_active)
        return _profiles_queue(PROFILES_OP_COPY, profile, from);

    // Into the live profile: it takes over now, as a switch would
    const profile_s *source = _profiles_stored(from);
    if (!source)
        return false;
    _profiles_apply(source, true);
    _profiles_active_dirty = true;
    _profiles_global_dirty = true;
    return true;
}

static bool _profiles_rename(uint8_t profile, const uint8_t *name, uint32_t len)
{
    memset(_profiles_names[profile], 0, PROFILES_NAME_LEN);
    memcpy(_profiles_names[profile], name, (len < PROFILES_NAME_LEN) ? len : PROFILES_NAME_LEN);
    if (profile == _profiles_active)
    {
        _profiles_active_dirty = true;
        return true;
    }
    return _profiles_queue(PROFILES_OP_RENAME, profile, 0);
}

void profiles_webusb(const uint8_t *data, uint32_t size)
{
    uint8_t reply[64] = {0};
    const profiles_op_t op = (size > 1) ? (profiles_op_t)data[1] : PROFILES_OP_INFO;
    const uint8_t profile = (size > 2) ? data[2] : 0;
    bool done = false;

    if (_profiles_ready && (profile < PROFILES_COUNT))
    {
        switch (op)
        {
            case PROFILES_OP_INFO:
                done = true;
                break;

            case PROFILES_OP_SWITCH:
                done = _profiles_switch(profile);
                break;

            case PROFILES_OP_RENAME:
                // No name clears it (the app names it then)
                done = (size >= 4) && _profiles_rename(profile, &data[4], size - 4u);
                break;

            case PROFILES_OP_COPY:
                done = (size > 3) && (data[3] < PROFILES_COUNT) && _profiles_copy(profile, data[3]);
                break;

            default:
                break;
        }

        // Answered once stored
        if (done && (op != PROFILES_OP_INFO))
            done = _profiles_flush();
    }

    reply[0] = WEBUSB_ID_PROFILE;
    reply[1] = (uint8_t)op;
    reply[2] = done ? 1 : 0;
    reply[3] = _profiles_ready ? PROFILES_COUNT : 0;
    reply[4] = _profiles_active;
    reply[5] = profile;
    if (_profiles_ready && (profile < PROFILES_COUNT))
        memcpy(&reply[6], _profiles_names[profile], PROFILES_NAME_LEN);
    webusb_send_bulk(reply, (uint16_t)(6u + PROFILES_NAME_LEN));
}

#else

bool profiles_init(void)
{
    return false;
}

bool profiles_save(void)
{
    return false;
}

void profiles_task(uint64_t now_us)
{
    (void)now_us;
}

// Without profiles: the app hears a count of 0
void profiles_webusb(const uint8_t *data, uint32_t size)
{
    uint8_t reply[6] = {WEBUSB_ID_PROFILE, (size > 1) ? data[1] : 0, 0, 0, 0, 0};
    webusb_send_bulk(reply, sizeof(reply));
}

#endif
