#include "utilities/profiles.h"

#include <stddef.h>
#include <string.h>

#include "hal/record_hal.h"
#include "usb/webusb.h"
#include "utilities/settings.h"

#define PROFILES_RECORD_GLOBAL 0u
#define PROFILES_RECORD_FIRST  1u // Profile n is record 1 + n

// A failed write is tried again on the next run (record_hal moves on to another sector). This many
// in a row stop the writing, and fail the app's request, until the next save or request.
#define PROFILES_WRITE_TRIES 3u

// WEBUSB_ID_PROFILE requests and replies (profiles.h)
#define PROFILES_REQUEST_FROM_AT  3u // COPY's source
#define PROFILES_REQUEST_NAME_AT  4u // RENAME's name
#define PROFILES_REPLY_HEADER_LEN 6u

// The bytes from one field of a struct to another, both included
#define PROFILES_SPAN(type, first, last) \
    (offsetof(type, last) + sizeof(((type *)0)->last) - offsetof(type, first))

// The profile's parts of the blocks, each one stretch of bytes: from the left stick's angle map to the
// right stick's curve, from motion on/off to the per-mode mask, haptic strength and trigger rumble
#define PROFILES_ANALOG_AT  offsetof(analogConfig_s, joy_config_l)
#define PROFILES_ANALOG_LEN PROFILES_SPAN(analogConfig_s, joy_config_l, r_exp_scaler)
#define PROFILES_IMU_AT     offsetof(imuConfig_s, imu_disabled)
#define PROFILES_IMU_LEN    PROFILES_SPAN(imuConfig_s, imu_disabled, imu_mode_disable_mask)
#define PROFILES_HAPTIC_AT  offsetof(hapticConfig_s, haptic_strength)
#define PROFILES_HAPTIC_LEN PROFILES_SPAN(hapticConfig_s, haptic_strength, haptic_triggers)

typedef struct __attribute__((packed))
{
    uint8_t active;
    uint8_t reserved[3];
} profiles_global_head_s; // The global record: this, then all of live_settings

// A profile's record
typedef struct __attribute__((packed))
{
    uint8_t name[PROFILES_NAME_LEN];
    uint8_t analog[PROFILES_ANALOG_LEN];
    uint8_t imu[PROFILES_IMU_LEN];
    uint8_t haptic[PROFILES_HAPTIC_LEN];
    uint8_t rgb[RGB_CFB_SIZE];
    uint8_t input[INPUT_CFB_SIZE];
} profile_s;

// One of the profile's stretches of a block
typedef struct
{
    cfg_block_t block;
    uint8_t *live; // In live_settings
    uint16_t at;   // In profile_s
    uint16_t len;
} profiles_slice_s;

// The app's request, run from the task
typedef struct
{
    bool waiting;                    // Not answered yet
    profiles_op_t op;
    uint8_t profile;
    uint8_t source;                  // COPY's source, else the profile itself
    bool apply;                      // source's stored profile goes live (SWITCH, COPY into the active)
    bool write;                      // profile's record, from source (RENAME, COPY into another)
    uint8_t name[PROFILES_NAME_LEN]; // The name the record takes
} profiles_request_s;

typedef enum
{
    PROFILES_STEP_IDLE, // Nothing waits
    PROFILES_STEP_WROTE,
    PROFILES_STEP_FAILED,
} profiles_step_t;

static const profiles_slice_s _profiles_slices[] = {
    {CFG_BLOCK_ANALOG, &live_settings.analog_configuration_block[PROFILES_ANALOG_AT],
     offsetof(profile_s, analog), PROFILES_ANALOG_LEN},
    {CFG_BLOCK_IMU, &live_settings.imu_configuration_block[PROFILES_IMU_AT], offsetof(profile_s, imu),
     PROFILES_IMU_LEN},
    {CFG_BLOCK_HAPTIC, &live_settings.haptic_configuration_block[PROFILES_HAPTIC_AT],
     offsetof(profile_s, haptic), PROFILES_HAPTIC_LEN},
    {CFG_BLOCK_RGB, live_settings.rgb_configuration_block, offsetof(profile_s, rgb), RGB_CFB_SIZE},
    {CFG_BLOCK_INPUT, live_settings.input_configuration_block, offsetof(profile_s, input), INPUT_CFB_SIZE},
};
#define PROFILES_SLICES    (sizeof(_profiles_slices) / sizeof(_profiles_slices[0]))
#define PROFILES_PARTS_LEN (1u + PROFILES_SLICES) // The name, then the slices

_Static_assert(PROFILES_RECORD_FIRST + PROFILES_COUNT <= RECORD_HAL_IDS, "a record id for each profile");
_Static_assert(PROFILES_COUNT <= 8u, "a bit per profile in _profiles_dirty_mask");
_Static_assert(PROFILES_PARTS_LEN <= RECORD_HAL_PARTS_MAX, "a profile's record in one write");
_Static_assert(sizeof(profile_s) <= RECORD_HAL_PAYLOAD_MAX_LEN, "a profile fills more than its record");
_Static_assert(sizeof(profiles_global_head_s) + sizeof(settings_live_s) <= RECORD_HAL_PAYLOAD_MAX_LEN,
               "the global settings fill more than their record");

static bool _profiles_ready = false;
static uint8_t _profiles_active = 0;
static uint8_t _profiles_names[PROFILES_COUNT][PROFILES_NAME_LEN];
static uint8_t _profiles_dirty_mask = 0; // Profiles to store from the live settings (bit per profile)
static bool _profiles_global_dirty = false;
static uint8_t _profiles_tries = 0; // Failed writes in a row
static profiles_request_s _profiles_request;

// --- Records ---

static const profile_s *_profiles_stored(uint8_t profile)
{
    uint32_t len = 0;
    const uint8_t *p = record_hal_find((uint16_t)(PROFILES_RECORD_FIRST + profile), &len);
    return (p && (len == sizeof(profile_s))) ? (const profile_s *)p : NULL;
}

// A stored profile's slices into the live settings. Running, what reads each block takes it up.
static void _profiles_load(const profile_s *p, bool running)
{
    for (uint8_t i = 0; i < PROFILES_SLICES; i++)
    {
        const profiles_slice_s *s = &_profiles_slices[i];
        memcpy(s->live, (const uint8_t *)p + s->at, s->len);
        if (running)
            settings_reinit_block(s->block);
    }
}

// profile's record under name: the slices from the live settings when source is the active profile,
// else from source's stored record
static bool _profiles_write(uint8_t profile, const uint8_t *name, uint8_t source)
{
    const profile_s *stored = NULL;
    if (source != _profiles_active)
    {
        stored = _profiles_stored(source);
        if (!stored)
            return false;
    }

    record_hal_part_s parts[PROFILES_PARTS_LEN];
    parts[0] = (record_hal_part_s){.data = name, .len = PROFILES_NAME_LEN};
    for (uint8_t i = 0; i < PROFILES_SLICES; i++)
    {
        const profiles_slice_s *s = &_profiles_slices[i];
        parts[1u + i] = (record_hal_part_s){
            .data = stored ? (const uint8_t *)stored + s->at : s->live,
            .len = s->len,
        };
    }
    return record_hal_write((uint16_t)(PROFILES_RECORD_FIRST + profile), parts, PROFILES_PARTS_LEN);
}

static bool _profiles_write_global(void)
{
    const profiles_global_head_s head = {.active = _profiles_active};
    const record_hal_part_s parts[] = {
        {.data = &head, .len = sizeof(head)},
        {.data = &live_settings, .len = sizeof(settings_live_s)},
    };
    return record_hal_write(PROFILES_RECORD_GLOBAL, parts, (uint8_t)(sizeof(parts) / sizeof(parts[0])));
}

// The next record waiting: the profiles stored from the live settings, then the request's own, then
// the global record
static profiles_step_t _profiles_write_next(void)
{
    profiles_request_s *r = &_profiles_request;

    if (_profiles_dirty_mask)
    {
        const uint8_t profile = (uint8_t)__builtin_ctz(_profiles_dirty_mask);
        if (!_profiles_write(profile, _profiles_names[profile], _profiles_active))
            return PROFILES_STEP_FAILED;
        _profiles_dirty_mask &= (uint8_t)~(1u << profile);
        return PROFILES_STEP_WROTE;
    }

    if (r->write)
    {
        if (!_profiles_write(r->profile, r->name, r->source))
            return PROFILES_STEP_FAILED;
        memcpy(_profiles_names[r->profile], r->name, PROFILES_NAME_LEN);
        r->write = false;
        return PROFILES_STEP_WROTE;
    }

    if (_profiles_global_dirty)
    {
        if (!_profiles_write_global())
            return PROFILES_STEP_FAILED;
        _profiles_global_dirty = false;
        return PROFILES_STEP_WROTE;
    }
    return PROFILES_STEP_IDLE;
}

// --- The app's requests ---

static void _profiles_reply(profiles_op_t op, uint8_t profile, bool done)
{
    uint8_t reply[PROFILES_REPLY_HEADER_LEN + PROFILES_NAME_LEN] = {0};
    reply[0] = WEBUSB_ID_PROFILE;
    reply[1] = (uint8_t)op;
    reply[2] = done ? 1u : 0u;
    reply[3] = _profiles_ready ? PROFILES_COUNT : 0u;
    reply[4] = _profiles_active;
    reply[5] = profile;
    if (_profiles_ready && (profile < PROFILES_COUNT))
        memcpy(&reply[PROFILES_REPLY_HEADER_LEN], _profiles_names[profile], PROFILES_NAME_LEN);
    webusb_send_bulk(reply, sizeof(reply));
}

static void _profiles_answer(bool done)
{
    profiles_request_s *r = &_profiles_request;
    if (!r->waiting)
        return;
    _profiles_reply(r->op, r->profile, done);
    memset(r, 0, sizeof(*r));
}

// The app's request, set to run from the task. False when it can't be done.
static bool _profiles_request_set(profiles_op_t op, uint8_t profile, const uint8_t *data, uint32_t size)
{
    profiles_request_s *r = &_profiles_request;
    *r = (profiles_request_s){.op = op, .profile = profile, .source = profile};
    memcpy(r->name, _profiles_names[profile], PROFILES_NAME_LEN);

    switch (op)
    {
        case PROFILES_OP_SWITCH:
            // The profile left is stored first
            if (profile != _profiles_active)
            {
                _profiles_dirty_mask |= (uint8_t)(1u << _profiles_active);
                r->apply = true;
            }
            break;

        case PROFILES_OP_RENAME:
        {
            // No name clears it (the app names it then)
            if (size < PROFILES_REQUEST_NAME_AT)
                return false;
            const uint32_t len = size - PROFILES_REQUEST_NAME_AT;
            memset(r->name, 0, PROFILES_NAME_LEN);
            memcpy(r->name, &data[PROFILES_REQUEST_NAME_AT],
                   (len < PROFILES_NAME_LEN) ? len : PROFILES_NAME_LEN);
            r->write = true;
            break;
        }

        case PROFILES_OP_COPY:
            if ((size <= PROFILES_REQUEST_FROM_AT) || (data[PROFILES_REQUEST_FROM_AT] >= PROFILES_COUNT) ||
                (data[PROFILES_REQUEST_FROM_AT] == profile))
                return false;
            r->source = data[PROFILES_REQUEST_FROM_AT];
            // Into the active profile: it takes over now, as a switch would
            r->apply = (profile == _profiles_active);
            r->write = !r->apply;
            break;

        default:
            return false;
    }

    r->waiting = true;
    _profiles_tries = 0;
    return true;
}

// The request's source goes live once nothing waits to be stored from the live settings (the
// profile a switch leaves, first). False when it has no record.
static bool _profiles_request_apply(void)
{
    profiles_request_s *r = &_profiles_request;
    const profile_s *p = _profiles_stored(r->source);
    if (!p)
        return false;

    _profiles_load(p, true);
    r->apply = false;
    if (r->op == PROFILES_OP_SWITCH)
    {
        _profiles_active = r->profile;
        _profiles_global_dirty = true;
    }
    else
    {
        // A copy into the active profile is stored from what is now live, under its own name
        _profiles_dirty_mask |= (uint8_t)(1u << _profiles_active);
    }
    return true;
}

static bool _profiles_pending(void)
{
    const profiles_request_s *r = &_profiles_request;
    return _profiles_dirty_mask || _profiles_global_dirty || r->apply || r->write;
}

// --- Public ---

void profiles_init(void)
{
    _profiles_ready = record_hal_init();
    if (!_profiles_ready)
        return;

    uint32_t len = 0;
    const uint8_t *global = record_hal_find(PROFILES_RECORD_GLOBAL, &len);
    if (global && (len == sizeof(profiles_global_head_s) + sizeof(settings_live_s)))
    {
        const profiles_global_head_s *head = (const profiles_global_head_s *)global;
        _profiles_active = (head->active < PROFILES_COUNT) ? head->active : 0;
        memcpy(&live_settings, &global[sizeof(profiles_global_head_s)], sizeof(settings_live_s));
    }
    else
    {
        // No global record (the first boot with profiles): the settings page as it was becomes it
        _profiles_active = 0;
        _profiles_global_dirty = true;
    }

    // A profile without a record (the first boot, or a power cut before it was written) starts as a
    // copy of the active one, unnamed
    memset(_profiles_names, 0, sizeof(_profiles_names));
    for (uint8_t i = 0; i < PROFILES_COUNT; i++)
    {
        const profile_s *p = _profiles_stored(i);
        if (p)
            memcpy(_profiles_names[i], p->name, PROFILES_NAME_LEN);
        else
            _profiles_dirty_mask |= (uint8_t)(1u << i);
    }

    const profile_s *active = _profiles_stored(_profiles_active);
    if (active)
        _profiles_load(active, false);
}

bool profiles_save(void)
{
    if (!_profiles_ready)
        return false;
    _profiles_dirty_mask |= (uint8_t)(1u << _profiles_active);
    _profiles_global_dirty = true;
    _profiles_tries = 0;
    return true;
}

void profiles_task(uint64_t now_us)
{
    (void)now_us;
    if (!_profiles_ready || (_profiles_tries >= PROFILES_WRITE_TRIES))
        return;

    if (_profiles_request.apply && !_profiles_dirty_mask && !_profiles_request_apply())
    {
        _profiles_answer(false);
        return;
    }

    switch (_profiles_write_next())
    {
        case PROFILES_STEP_FAILED:
            if (++_profiles_tries >= PROFILES_WRITE_TRIES)
                _profiles_answer(false);
            return;

        case PROFILES_STEP_WROTE:
            _profiles_tries = 0;
            break;

        default:
            break;
    }

    if (!_profiles_pending())
        _profiles_answer(true);
}

void profiles_webusb(const uint8_t *data, uint32_t size)
{
    const profiles_op_t op = (size > 1) ? (profiles_op_t)data[1] : PROFILES_OP_INFO;
    const uint8_t profile = (size > 2) ? data[2] : 0;
    bool done = false;

    if (_profiles_ready && (profile < PROFILES_COUNT))
    {
        if (op == PROFILES_OP_INFO)
            done = true;
        else if (!_profiles_request.waiting && _profiles_request_set(op, profile, data, size))
            return; // Answered from the task once stored
    }
    _profiles_reply(op, profile, done);
}
