/**
 * @file nwii_lib_protocol.c
 * @brief Wii Remote report protocol: output report handling, register/EEPROM access, extension
 *        hotplug and encryption, and input report generation for every data reporting mode.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo, Wii and
 * related marks are trademarks of their respective owners.
 */

#include "nwii_lib.h"
#include "nwii_lib_protocol.h"
#include "nwii_lib_extension.h"
#include "nwii_lib_crypto.h"
#include "nwii_lib_motionplus.h"

#include <stddef.h>
#include <string.h>

/* Queue depths. The host bursts several register writes while configuring the IR camera or an
 * extension, and the remote answers each one, so both sides need some slack. */
#define NWII_OUT_QUEUE_LEN          16u
#define NWII_RESP_QUEUE_LEN         16u

/* Reports to wait between an extension unplug and the next plug-in (~240 ms at 125 Hz). */
#define NWII_HOTPLUG_DELAY_REPORTS  30u

/* EEPROM: 0x1700 addressable bytes. Only the calibration area is backed by RAM. */
#define NWII_EEPROM_SIZE            0x1700u
#define NWII_EEPROM_RAM_LEN         0x30u

/* Memory access error codes (low nibble of report 0x21, ack result of 0x16) */
#define NWII_ERROR_OK               0x00u
#define NWII_ERROR_NO_DEVICE        0x07u
#define NWII_ERROR_BAD_ADDRESS      0x08u

/* Register spaces (address bits 16..23, bit 16 ignored) */
#define NWII_SPACE_SPEAKER          0xA2u
#define NWII_SPACE_EXTENSION        0xA4u
#define NWII_SPACE_MOTIONPLUS       0xA6u
#define NWII_SPACE_CAMERA           0xB0u

/* Output report flag bits in byte 1 */
#define NWII_OUT_FLAG_RUMBLE        0x01u
#define NWII_OUT_FLAG_ACK           0x02u
#define NWII_OUT_FLAG_ENABLE        0x04u
#define NWII_OUT_FLAG_REGISTERS     0x0Cu

/* Remote accelerometer calibration, matching the EEPROM image below (10-bit) */
#define NWII_REMOTE_ACCEL_ZERO      0x214u
#define NWII_REMOTE_ACCEL_ONE_G     0x280u

/* EEPROM calibration area of a real remote: IR calibration (0x00..0x15) and accelerometer
 * calibration (0x16..0x1F), each followed by a backup copy. */
static const uint8_t _nwii_eeprom_default[NWII_EEPROM_RAM_LEN] = {
    0x66, 0x4D, 0x08, 0x66, 0xB3, 0x9A, 0xB3, 0xB3, 0x9A, 0x4D, 0x97, 0x66, 0x4D, 0x08, 0x66, 0xB3,
    0x9A, 0xB3, 0xB3, 0x9A, 0x4D, 0x97, 0x85, 0x85, 0x85, 0x00, 0xA0, 0xA0, 0xA0, 0x00, 0x40, 0x04,
    0x85, 0x85, 0x85, 0x00, 0xA0, 0xA0, 0xA0, 0x00, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

typedef struct
{
    uint8_t len;
    uint8_t data[NWII_OUTPUT_REPORT_MAX];
} nwii_out_entry_s;

typedef enum
{
    NWII_RESP_ACK,
    NWII_RESP_STATUS,
    NWII_RESP_READ,
} nwii_resp_kind_t;

typedef struct
{
    uint8_t  kind;
    uint8_t  report;    // ACK: acknowledged report id
    uint8_t  error;     // ACK: result code
    bool     registers; // READ: register space (true) or EEPROM (false)
    uint32_t addr;      // READ: next address to send
    uint16_t remaining; // READ: bytes left
} nwii_resp_entry_s;

typedef struct
{
    uint8_t mode;
    bool    ir_enabled;
    bool    speaker_enabled;
    bool    interleave_second; // Next 0x3E/0x3F report is the 0x3F half

    nwii_extension_t ext_attached; // Plugged into the port (behind the MotionPlus, if any)
    uint8_t          hotplug_timer;
    bool             ext_detect;   // Extension detect line, as last reported in a status report

    uint8_t ext_reg[NWII_EXTENSION_REG_SIZE];
    uint8_t camera_reg[0x40];
    uint8_t speaker_reg[0x10];
    uint8_t eeprom[NWII_EEPROM_RAM_LEN];

    nwii_crypto_state_s crypto;

    nwii_resp_entry_s resp[NWII_RESP_QUEUE_LEN];
    uint8_t resp_head;
    uint8_t resp_tail;
} nwii_protocol_state_s;

static nwii_protocol_state_s _nwii;

/* Written from the tunnel context only */
static bool    _nwii_rumble = false;
static uint8_t _nwii_leds = 0;

/* Built-in MotionPlus, from nwii_device_config_s */
static bool _nwii_mp_enabled = false;

/* Extension requested by the firmware; applied by the generator's hotplug step */
static volatile nwii_extension_t _nwii_ext_requested = NWII_EXTENSION_NONE;

/* Single producer (tunnel) / single consumer (generator) hand-off of host output reports */
static nwii_out_entry_s   _nwii_out_queue[NWII_OUT_QUEUE_LEN];
static volatile uint8_t   _nwii_out_head = 0;
static volatile uint8_t   _nwii_out_tail = 0;

static inline void _nwii_memory_barrier(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __sync_synchronize();
#endif
}

/* --- Response queue (generator context only) --- */

static nwii_resp_entry_s *_nwii_resp_push(void)
{
    const uint8_t next = (uint8_t)((_nwii.resp_head + 1u) % NWII_RESP_QUEUE_LEN);
    if (next == _nwii.resp_tail)
    {
        return NULL;
    }

    nwii_resp_entry_s *entry = &_nwii.resp[_nwii.resp_head];
    memset(entry, 0, sizeof(*entry));
    _nwii.resp_head = next;
    return entry;
}

static inline nwii_resp_entry_s *_nwii_resp_peek(void)
{
    return (_nwii.resp_tail == _nwii.resp_head) ? NULL : &_nwii.resp[_nwii.resp_tail];
}

static inline void _nwii_resp_pop(void)
{
    if (_nwii.resp_tail != _nwii.resp_head)
    {
        _nwii.resp_tail = (uint8_t)((_nwii.resp_tail + 1u) % NWII_RESP_QUEUE_LEN);
    }
}

static void _nwii_queue_ack(uint8_t report, uint8_t error)
{
    nwii_resp_entry_s *entry = _nwii_resp_push();
    if (!entry) return;
    entry->kind = NWII_RESP_ACK;
    entry->report = report;
    entry->error = error;
}

static void _nwii_queue_status(void)
{
    nwii_resp_entry_s *entry = _nwii_resp_push();
    if (!entry) return;
    entry->kind = NWII_RESP_STATUS;
}

static void _nwii_queue_read(bool registers, uint32_t addr, uint16_t size)
{
    if (size == 0u) return;

    nwii_resp_entry_s *entry = _nwii_resp_push();
    if (!entry) return;
    entry->kind = NWII_RESP_READ;
    entry->registers = registers;
    entry->addr = addr;
    entry->remaining = size;
}

/* --- Extension state --- */

/* The extension port answers the host directly unless the MotionPlus has taken it over */
static inline bool _nwii_port_direct(void)
{
    const nwii_mp_status_t mp = nwii_mp_status();
    return (mp == NWII_MP_ABSENT) || (mp == NWII_MP_INACTIVE);
}

static inline bool _nwii_ext_encrypted(void)
{
    return _nwii_port_direct() && (_nwii.ext_attached != NWII_EXTENSION_NONE) &&
           (_nwii.ext_reg[NWII_EXTENSION_REG_CRYPT] == NWII_EXTENSION_CRYPT_ON);
}

static void _nwii_ext_attach(nwii_extension_t extension)
{
    _nwii.ext_attached = extension;
    nwii_extension_reset_registers(_nwii.ext_reg, extension);
    memset(&_nwii.crypto, 0, sizeof(_nwii.crypto));
}

/* A different extension is unplugged first, then plugged in after a short delay so the host sees
 * both edges (see _nwii_detect_step). */
static void _nwii_hotplug_step(void)
{
    const nwii_extension_t want = _nwii_ext_requested;

    if (want == _nwii.ext_attached)
    {
        return;
    }

    if (_nwii.ext_attached != NWII_EXTENSION_NONE)
    {
        _nwii_ext_attach(NWII_EXTENSION_NONE);
        _nwii.hotplug_timer = NWII_HOTPLUG_DELAY_REPORTS;
        return;
    }

    if (_nwii.hotplug_timer > 0u)
    {
        _nwii.hotplug_timer--;
        return;
    }

    _nwii_ext_attach(want);
}

/* Real remotes send an unsolicited status report whenever the extension detect line changes: an
 * extension plugged in or out, or the MotionPlus switching over. */
static void _nwii_detect_step(void)
{
    const bool detect = nwii_mp_detect_pin(_nwii.ext_attached != NWII_EXTENSION_NONE);
    if (detect != _nwii.ext_detect)
    {
        _nwii.ext_detect = detect;
        _nwii_queue_status();
    }
}

/* --- Memory access --- */

static uint8_t _nwii_write_registers(uint32_t addr, const uint8_t *src, uint8_t size)
{
    const uint8_t offset = (uint8_t)(addr & 0xFFu);

    switch ((uint8_t)((addr >> 16) & 0xFEu))
    {
    case NWII_SPACE_SPEAKER:
        for (uint8_t i = 0; i < size; i++)
            _nwii.speaker_reg[(offset + i) % sizeof(_nwii.speaker_reg)] = src[i];
        return NWII_ERROR_OK;

    case NWII_SPACE_CAMERA:
        for (uint8_t i = 0; i < size; i++)
            _nwii.camera_reg[(offset + i) % sizeof(_nwii.camera_reg)] = src[i];
        return NWII_ERROR_OK;

    case NWII_SPACE_EXTENSION:
    {
        switch (nwii_mp_status())
        {
        case NWII_MP_ACTIVE:
            // A write to 0xF0 deactivates the MotionPlus and also initializes the extension behind it
            if (!nwii_mp_write_active(offset, src, size) || (_nwii.ext_attached == NWII_EXTENSION_NONE))
                return NWII_ERROR_OK;
            break;

        case NWII_MP_ACTIVATING:
        case NWII_MP_DEACTIVATING:
            return NWII_ERROR_NO_DEVICE;

        default:
            break;
        }

        if (_nwii.ext_attached == NWII_EXTENSION_NONE)
            return NWII_ERROR_NO_DEVICE;

        for (uint8_t i = 0; i < size; i++)
            _nwii.ext_reg[(uint8_t)(offset + i)] = src[i];

        // Any write into the key area re-derives the cipher; only the final full key matters.
        const uint16_t end = (uint16_t)offset + size;
        if (end > NWII_EXTENSION_REG_KEY && offset < (NWII_EXTENSION_REG_KEY + 16u))
        {
            nwii_crypto_generate(&_nwii.crypto, &_nwii.ext_reg[NWII_EXTENSION_REG_KEY]);
        }
        return NWII_ERROR_OK;
    }

    case NWII_SPACE_MOTIONPLUS:
        // The MotionPlus answers here only while inactive
        if (nwii_mp_status() != NWII_MP_INACTIVE)
            return NWII_ERROR_NO_DEVICE;
        nwii_mp_write_inactive(offset, src, size);
        return NWII_ERROR_OK;

    default:
        return NWII_ERROR_NO_DEVICE;
    }
}

static uint8_t _nwii_write_eeprom(uint32_t addr, const uint8_t *src, uint8_t size)
{
    const uint16_t offset = (uint16_t)(addr & 0xFFFFu);

    if ((uint32_t)offset + size > NWII_EEPROM_SIZE)
        return NWII_ERROR_BAD_ADDRESS;

    // Writes outside the calibration area (e.g. Mii transfers) are accepted and discarded
    for (uint8_t i = 0; i < size; i++)
    {
        const uint16_t pos = (uint16_t)(offset + i);
        if (pos < NWII_EEPROM_RAM_LEN)
            _nwii.eeprom[pos] = src[i];
    }
    return NWII_ERROR_OK;
}

/* Fill one read chunk. Returns the error code for the chunk. */
static uint8_t _nwii_read_chunk(bool registers, uint32_t addr, uint8_t *dst, uint8_t size)
{
    if (!registers)
    {
        const uint16_t offset = (uint16_t)(addr & 0xFFFFu);
        if ((uint32_t)offset + size > NWII_EEPROM_SIZE)
            return NWII_ERROR_BAD_ADDRESS;

        for (uint8_t i = 0; i < size; i++)
        {
            const uint16_t pos = (uint16_t)(offset + i);
            dst[i] = (pos < NWII_EEPROM_RAM_LEN) ? _nwii.eeprom[pos] : 0x00u;
        }
        return NWII_ERROR_OK;
    }

    const uint8_t offset = (uint8_t)(addr & 0xFFu);

    switch ((uint8_t)((addr >> 16) & 0xFEu))
    {
    case NWII_SPACE_SPEAKER:
        for (uint8_t i = 0; i < size; i++)
            dst[i] = _nwii.speaker_reg[(offset + i) % sizeof(_nwii.speaker_reg)];
        return NWII_ERROR_OK;

    case NWII_SPACE_CAMERA:
        for (uint8_t i = 0; i < size; i++)
            dst[i] = _nwii.camera_reg[(offset + i) % sizeof(_nwii.camera_reg)];
        return NWII_ERROR_OK;

    case NWII_SPACE_EXTENSION:
        if (nwii_mp_status() == NWII_MP_ACTIVE)
        {
            nwii_mp_read(offset, dst, size);
            return NWII_ERROR_OK;
        }

        if (!_nwii_port_direct() || (_nwii.ext_attached == NWII_EXTENSION_NONE))
            return NWII_ERROR_NO_DEVICE;

        for (uint8_t i = 0; i < size; i++)
            dst[i] = _nwii.ext_reg[(uint8_t)(offset + i)];

        if (_nwii_ext_encrypted())
            nwii_crypto_encrypt(&_nwii.crypto, dst, offset, size);
        return NWII_ERROR_OK;

    case NWII_SPACE_MOTIONPLUS:
        if (nwii_mp_status() != NWII_MP_INACTIVE)
            return NWII_ERROR_NO_DEVICE;
        nwii_mp_read(offset, dst, size);
        return NWII_ERROR_OK;

    default:
        return NWII_ERROR_NO_DEVICE;
    }
}

/* --- Output report processing (generator context) --- */

static inline uint32_t _nwii_read_be24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static bool _nwii_mode_valid(uint8_t mode)
{
    return (mode >= 0x30u && mode <= 0x37u) || (mode >= 0x3Du && mode <= 0x3Fu);
}

static void _nwii_process_outputreport(const uint8_t *data, uint8_t len)
{
    const uint8_t id = data[0];
    const uint8_t flags = data[1];
    const bool ack = (flags & NWII_OUT_FLAG_ACK) != 0u;

    switch (id)
    {
    case NWII_OUT_REPORT_MODE:
        if (len >= 3u && _nwii_mode_valid(data[2]))
        {
            _nwii.mode = data[2];
            _nwii.interleave_second = false;
        }
        if (ack) _nwii_queue_ack(id, NWII_ERROR_OK);
        break;

    case NWII_OUT_IR_ENABLE:
    case NWII_OUT_IR_ENABLE_2:
        _nwii.ir_enabled = (flags & NWII_OUT_FLAG_ENABLE) != 0u;
        if (ack) _nwii_queue_ack(id, NWII_ERROR_OK);
        break;

    case NWII_OUT_SPEAKER_ENABLE:
        _nwii.speaker_enabled = (flags & NWII_OUT_FLAG_ENABLE) != 0u;
        if (ack) _nwii_queue_ack(id, NWII_ERROR_OK);
        break;

    case NWII_OUT_STATUS_REQUEST:
        _nwii_queue_status();
        break;

    case NWII_OUT_WRITE_MEMORY:
    {
        // flags, address (24-bit BE), size, 16 data bytes
        if (len < 7u) break;

        uint8_t size = data[5];
        if (size > 16u) size = 16u;
        if ((uint16_t)size + 6u > len) size = (uint8_t)(len - 6u);

        const uint32_t addr = _nwii_read_be24(&data[2]);
        const uint8_t error = (flags & NWII_OUT_FLAG_REGISTERS)
                                  ? _nwii_write_registers(addr, &data[6], size)
                                  : _nwii_write_eeprom(addr, &data[6], size);

        // Memory writes are always acknowledged
        _nwii_queue_ack(id, error);
        break;
    }

    case NWII_OUT_READ_MEMORY:
    {
        // flags, address (24-bit BE), size (16-bit BE)
        if (len < 7u) break;

        const uint32_t addr = _nwii_read_be24(&data[2]);
        const uint16_t size = (uint16_t)(((uint16_t)data[5] << 8) | data[6]);
        _nwii_queue_read((flags & NWII_OUT_FLAG_REGISTERS) != 0u, addr, size);
        break;
    }

    case NWII_OUT_RUMBLE:
    case NWII_OUT_LEDS:
    case NWII_OUT_SPEAKER_MUTE:
    default:
        // Rumble and LEDs were applied when the report arrived
        if (ack) _nwii_queue_ack(id, NWII_ERROR_OK);
        break;
    }
}

/* --- Input report building --- */

static void _nwii_put_buttons(uint8_t out[2], const nwii_input_s *in)
{
    out[0] = (uint8_t)((in->remote.left  ? 0x01u : 0u) |
                       (in->remote.right ? 0x02u : 0u) |
                       (in->remote.down  ? 0x04u : 0u) |
                       (in->remote.up    ? 0x08u : 0u) |
                       (in->remote.plus  ? 0x10u : 0u));

    out[1] = (uint8_t)((in->remote.two   ? 0x01u : 0u) |
                       (in->remote.one   ? 0x02u : 0u) |
                       (in->remote.b     ? 0x04u : 0u) |
                       (in->remote.a     ? 0x08u : 0u) |
                       (in->remote.minus ? 0x10u : 0u) |
                       (in->remote.home  ? 0x80u : 0u));
}

static void _nwii_accel_10bit(const nwii_input_s *in, uint16_t *x, uint16_t *y, uint16_t *z)
{
    *x = nwii_extension_accel_to_10bit(in->accel_x, NWII_REMOTE_ACCEL_ZERO, NWII_REMOTE_ACCEL_ONE_G);
    *y = nwii_extension_accel_to_10bit(in->accel_y, NWII_REMOTE_ACCEL_ZERO, NWII_REMOTE_ACCEL_ONE_G);
    *z = nwii_extension_accel_to_10bit(in->accel_z, NWII_REMOTE_ACCEL_ZERO, NWII_REMOTE_ACCEL_ONE_G);
}

/* Accelerometer MSBs follow the buttons; the LSBs ride in unused button bits (X keeps two, Y and
 * Z one each). */
static void _nwii_put_accel(uint8_t buttons[2], uint8_t out[3], const nwii_input_s *in)
{
    uint16_t x, y, z;
    _nwii_accel_10bit(in, &x, &y, &z);

    buttons[0] |= (uint8_t)((x & 0x03u) << 5);
    buttons[1] |= (uint8_t)(((y >> 1) & 0x01u) << 5);
    buttons[1] |= (uint8_t)(((z >> 1) & 0x01u) << 6);

    out[0] = (uint8_t)(x >> 2);
    out[1] = (uint8_t)(y >> 2);
    out[2] = (uint8_t)(z >> 2);
}

static inline bool _nwii_ir_point_visible(const nwii_ir_point_s *p)
{
    return _nwii.ir_enabled && p->visible && p->x < NWII_IR_RES_X && p->y < NWII_IR_RES_Y;
}

/* Basic mode: two pairs of points in 5 bytes each, no size. */
static void _nwii_put_ir_basic(uint8_t out[10], const nwii_input_s *in)
{
    for (uint8_t pair = 0; pair < 2u; pair++)
    {
        const nwii_ir_point_s *a = &in->ir[pair * 2u];
        const nwii_ir_point_s *b = &in->ir[pair * 2u + 1u];
        const uint16_t ax = _nwii_ir_point_visible(a) ? a->x : 0x3FFu;
        const uint16_t ay = _nwii_ir_point_visible(a) ? a->y : 0x3FFu;
        const uint16_t bx = _nwii_ir_point_visible(b) ? b->x : 0x3FFu;
        const uint16_t by = _nwii_ir_point_visible(b) ? b->y : 0x3FFu;

        uint8_t *p = &out[pair * 5u];
        p[0] = (uint8_t)ax;
        p[1] = (uint8_t)ay;
        p[2] = (uint8_t)(((ay >> 8) << 6) | ((ax >> 8) << 4) | ((by >> 8) << 2) | (bx >> 8));
        p[3] = (uint8_t)bx;
        p[4] = (uint8_t)by;
    }
}

/* Extended mode: four points of 3 bytes each with size. */
static void _nwii_put_ir_extended(uint8_t out[12], const nwii_input_s *in)
{
    for (uint8_t i = 0; i < NWII_IR_POINT_COUNT; i++)
    {
        uint8_t *p = &out[i * 3u];
        const nwii_ir_point_s *pt = &in->ir[i];

        if (!_nwii_ir_point_visible(pt))
        {
            p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF;
            continue;
        }

        p[0] = (uint8_t)pt->x;
        p[1] = (uint8_t)pt->y;
        p[2] = (uint8_t)(((pt->y >> 8) << 6) | ((pt->x >> 8) << 4) | (pt->size & 0x0Fu));
    }
}

/* Full mode: two points of 9 bytes each (extended data, bounding box, intensity). */
static void _nwii_put_ir_full(uint8_t out[18], const nwii_input_s *in, uint8_t first)
{
    for (uint8_t i = 0; i < 2u; i++)
    {
        uint8_t *p = &out[i * 9u];
        const nwii_ir_point_s *pt = &in->ir[first + i];

        if (!_nwii_ir_point_visible(pt))
        {
            memset(p, 0xFF, 9);
            continue;
        }

        const uint16_t size = pt->size & 0x0Fu;
        const uint16_t x0 = (pt->x > size) ? (uint16_t)(pt->x - size) : 0u;
        const uint16_t y0 = (pt->y > size) ? (uint16_t)(pt->y - size) : 0u;
        const uint16_t x1 = (uint16_t)(pt->x + size);
        const uint16_t y1 = (uint16_t)(pt->y + size);

        p[0] = (uint8_t)pt->x;
        p[1] = (uint8_t)pt->y;
        p[2] = (uint8_t)(((pt->y >> 8) << 6) | ((pt->x >> 8) << 4) | size);
        p[3] = (uint8_t)((x0 >> 3) & 0x7Fu);
        p[4] = (uint8_t)((y0 >> 3) & 0x7Fu);
        p[5] = (uint8_t)((x1 >> 3) & 0x7Fu);
        p[6] = (uint8_t)((y1 >> 3) & 0x7Fu);
        p[7] = 0x00;
        p[8] = 0x80; // Intensity
    }
}

/* Extension bytes are an I2C read of `len` bytes from register 0, encrypted like any other read. */
static void _nwii_put_extension(uint8_t *out, uint8_t len, const nwii_input_s *in)
{
    // An active MotionPlus sends its own 6-byte frames (never encrypted); the rest reads as zero
    if (nwii_mp_status() == NWII_MP_ACTIVE)
    {
        uint8_t frame[NWII_EXTENSION_DATA_LEN];
        nwii_mp_build(frame, in, _nwii.ext_attached);
        memset(out, 0x00, len);
        memcpy(out, frame, (len < NWII_EXTENSION_DATA_LEN) ? len : NWII_EXTENSION_DATA_LEN);
        return;
    }

    if (_nwii.ext_attached != NWII_EXTENSION_NONE)
    {
        nwii_extension_encode(_nwii.ext_attached, in, _nwii.ext_reg);
    }

    memcpy(out, _nwii.ext_reg, len);

    if (_nwii_ext_encrypted())
    {
        nwii_crypto_encrypt(&_nwii.crypto, out, 0, len);
    }
}

static uint8_t _nwii_build_status(uint8_t *data, const nwii_input_s *in)
{
    nwii_power_s power = {.level = 0xFF, .low = false};
    nwii_api_hook_get_power(&power);

    data[0] = NWII_IN_STATUS;
    _nwii_put_buttons(&data[1], in);
    data[3] = (uint8_t)((power.low ? 0x01u : 0u) |
                        (_nwii.ext_detect ? 0x02u : 0u) |
                        (_nwii.speaker_enabled ? 0x04u : 0u) |
                        (_nwii.ir_enabled ? 0x08u : 0u) |
                        ((_nwii_leds & 0x0Fu) << 4));
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = power.level;
    return 7u;
}

static uint8_t _nwii_build_ack(uint8_t *data, const nwii_input_s *in, const nwii_resp_entry_s *entry)
{
    data[0] = NWII_IN_ACK;
    _nwii_put_buttons(&data[1], in);
    data[3] = entry->report;
    data[4] = entry->error;
    return 5u;
}

/* One 0x21 packet per call: up to 16 bytes, tagged with the low 16 bits of its address. */
static uint8_t _nwii_build_read(uint8_t *data, const nwii_input_s *in, nwii_resp_entry_s *entry, bool *done)
{
    const uint8_t chunk = (entry->remaining > 16u) ? 16u : (uint8_t)entry->remaining;

    data[0] = NWII_IN_READ_DATA;
    _nwii_put_buttons(&data[1], in);
    memset(&data[6], 0x00, 16);

    const uint8_t error = _nwii_read_chunk(entry->registers, entry->addr, &data[6], chunk);

    if (error != NWII_ERROR_OK)
    {
        memset(&data[6], 0x00, 16);
        data[3] = (uint8_t)(0xF0u | error);
        *done = true;
    }
    else
    {
        data[3] = (uint8_t)(((chunk - 1u) & 0x0Fu) << 4);
        entry->remaining = (uint16_t)(entry->remaining - chunk);
        *done = (entry->remaining == 0u);
    }

    data[4] = (uint8_t)(entry->addr >> 8);
    data[5] = (uint8_t)(entry->addr);
    entry->addr += chunk;
    return 22u;
}

static uint8_t _nwii_build_data(uint8_t *data, const nwii_input_s *in)
{
    uint8_t mode = _nwii.mode;

    if (mode == 0x3Eu || mode == 0x3Fu)
    {
        mode = _nwii.interleave_second ? 0x3Fu : 0x3Eu;
        _nwii.interleave_second = !_nwii.interleave_second;
    }

    data[0] = mode;
    uint8_t *buttons = &data[1];
    _nwii_put_buttons(buttons, in);

    switch (mode)
    {
    default:
    case 0x30:
        data[0] = 0x30;
        return 3u;

    case 0x31:
        _nwii_put_accel(buttons, &data[3], in);
        return 6u;

    case 0x32:
        _nwii_put_extension(&data[3], 8, in);
        return 11u;

    case 0x33:
        _nwii_put_accel(buttons, &data[3], in);
        _nwii_put_ir_extended(&data[6], in);
        return 18u;

    case 0x34:
        _nwii_put_extension(&data[3], 19, in);
        return 22u;

    case 0x35:
        _nwii_put_accel(buttons, &data[3], in);
        _nwii_put_extension(&data[6], 16, in);
        return 22u;

    case 0x36:
        _nwii_put_ir_basic(&data[3], in);
        _nwii_put_extension(&data[13], 9, in);
        return 22u;

    case 0x37:
        _nwii_put_accel(buttons, &data[3], in);
        _nwii_put_ir_basic(&data[6], in);
        _nwii_put_extension(&data[16], 6, in);
        return 22u;

    case 0x3D:
        _nwii_put_extension(&data[1], 21, in);
        return 22u;

    case 0x3E:
    case 0x3F:
    {
        // Interleaved full IR: each half carries one accel axis and half of Z's 8 bits
        uint16_t x, y, z;
        _nwii_accel_10bit(in, &x, &y, &z);
        const uint8_t z8 = (uint8_t)(z >> 2);

        if (mode == 0x3Eu)
        {
            buttons[0] |= (uint8_t)(((z8 >> 4) & 0x03u) << 5);
            buttons[1] |= (uint8_t)(((z8 >> 6) & 0x03u) << 5);
            data[3] = (uint8_t)(x >> 2);
            _nwii_put_ir_full(&data[4], in, 0);
        }
        else
        {
            buttons[0] |= (uint8_t)((z8 & 0x03u) << 5);
            buttons[1] |= (uint8_t)(((z8 >> 2) & 0x03u) << 5);
            data[3] = (uint8_t)(y >> 2);
            _nwii_put_ir_full(&data[4], in, 2);
        }
        return 22u;
    }
    }
}

static void _nwii_default_input(nwii_input_s *in)
{
    memset(in, 0, sizeof(*in));

    // At rest, face up
    in->accel_z = NWII_ACCEL_1G_MG;
    in->nunchuk.accel_z = NWII_ACCEL_1G_MG;

    in->nunchuk.stick_x = NWII_STICK_CENTER;
    in->nunchuk.stick_y = NWII_STICK_CENTER;
    in->classic.ls_x = NWII_STICK_CENTER;
    in->classic.ls_y = NWII_STICK_CENTER;
    in->classic.rs_x = NWII_STICK_CENTER;
    in->classic.rs_y = NWII_STICK_CENTER;

    nwii_ir_clear(in->ir);
}

/* --- Protocol entry points --- */

void nwii_protocol_init(nwii_extension_t extension, bool motion_plus)
{
    _nwii_mp_enabled = motion_plus;
    _nwii_ext_requested = (extension < NWII_EXTENSION_MAX) ? extension : NWII_EXTENSION_NONE;
    nwii_protocol_connection_reset();
}

void nwii_protocol_connection_reset(void)
{
    memset(&_nwii, 0, sizeof(_nwii));

    _nwii.mode = NWII_IN_MODE_DEFAULT;
    memcpy(_nwii.eeprom, _nwii_eeprom_default, sizeof(_nwii.eeprom));

    // The extension is announced a moment after connecting, like a remote powering its port
    _nwii_ext_attach(NWII_EXTENSION_NONE);
    _nwii.hotplug_timer = NWII_HOTPLUG_DELAY_REPORTS;
    nwii_mp_reset(_nwii_mp_enabled);

    _nwii_out_tail = _nwii_out_head;
}

void nwii_protocol_set_extension(nwii_extension_t extension)
{
    if (extension >= NWII_EXTENSION_MAX) return;
    _nwii_ext_requested = extension;
}

nwii_extension_t nwii_protocol_get_extension(void)
{
    return _nwii_ext_requested;
}

void nwii_protocol_ingest_outputreport(const uint8_t *data, uint16_t len)
{
    // Every output report carries at least the rumble/flags byte
    if (data == NULL || len < 2u) return;

    const uint8_t id = data[0];
    if (id < NWII_OUT_RUMBLE || id > NWII_OUT_IR_ENABLE_2) return;

    const bool rumble = (data[1] & NWII_OUT_FLAG_RUMBLE) != 0u;
    if (rumble != _nwii_rumble)
    {
        _nwii_rumble = rumble;
        nwii_api_hook_set_rumble(rumble);
    }

    if (id == NWII_OUT_LEDS)
    {
        _nwii_leds = (uint8_t)(data[1] >> 4);
        nwii_api_hook_set_leds(_nwii_leds);
    }

    // Speaker audio streams continuously and never needs a reply
    if (id == NWII_OUT_SPEAKER_DATA) return;

    const uint8_t head = _nwii_out_head;
    const uint8_t next = (uint8_t)((head + 1u) % NWII_OUT_QUEUE_LEN);
    if (next == _nwii_out_tail) return; // Full; the host will retry

    if (len > NWII_OUTPUT_REPORT_MAX) len = NWII_OUTPUT_REPORT_MAX;
    memcpy(_nwii_out_queue[head].data, data, len);
    _nwii_out_queue[head].len = (uint8_t)len;

    _nwii_memory_barrier();
    _nwii_out_head = next;
}

bool nwii_protocol_generate_inputreport(uint8_t *data, uint8_t *len)
{
    if (data == NULL || len == NULL) return false;

    while (_nwii_out_tail != _nwii_out_head)
    {
        _nwii_memory_barrier();
        const uint8_t tail = _nwii_out_tail;
        _nwii_process_outputreport(_nwii_out_queue[tail].data, _nwii_out_queue[tail].len);
        _nwii_memory_barrier();
        _nwii_out_tail = (uint8_t)((tail + 1u) % NWII_OUT_QUEUE_LEN);
    }

    _nwii_hotplug_step();
    nwii_mp_step(_nwii.ext_attached, _nwii.ext_reg);
    _nwii_detect_step();

    nwii_input_s in;
    _nwii_default_input(&in);
    nwii_api_hook_get_input(&in);

    memset(data, 0, NWII_INPUT_REPORT_MAX);

    nwii_resp_entry_s *entry = _nwii_resp_peek();
    if (entry)
    {
        bool done = true;

        switch (entry->kind)
        {
        case NWII_RESP_ACK:
            *len = _nwii_build_ack(data, &in, entry);
            break;

        case NWII_RESP_STATUS:
            *len = _nwii_build_status(data, &in);
            break;

        case NWII_RESP_READ:
        default:
            *len = _nwii_build_read(data, &in, entry, &done);
            break;
        }

        if (done) _nwii_resp_pop();
        return true;
    }

    *len = _nwii_build_data(data, &in);
    return true;
}
