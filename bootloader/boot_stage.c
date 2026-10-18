#include "boot_stage.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"

#include "hal/rp2040/boot_layout.h"
#include "boot_flash.h"
#include "boot_rgb.h"

// --- Output: a sector at a time into the destination ---

typedef struct
{
    uint32_t offset;   // Next sector to write
    uint32_t fill;     // Bytes in the sector buffer
    uint32_t written;  // Bytes put so far
    uint32_t limit;    // The header's image size
} boot_stage_out_s;

static uint8_t __attribute__((aligned(4))) _boot_stage_sector[FLASH_SECTOR_SIZE];
static boot_stage_out_s _boot_stage_out;

static void _boot_stage_out_flush(void)
{
    if (_boot_stage_out.fill == 0)
        return;

    memset(&_boot_stage_sector[_boot_stage_out.fill], 0xFF, FLASH_SECTOR_SIZE - _boot_stage_out.fill);
    boot_flash_write_sector(_boot_stage_out.offset, _boot_stage_sector);
    _boot_stage_out.offset += FLASH_SECTOR_SIZE;
    _boot_stage_out.fill = 0;
    boot_rgb_task(BOOT_RGB_WRITING);
}

static bool _boot_stage_out_put(uint8_t value)
{
    if (_boot_stage_out.written >= _boot_stage_out.limit)
        return false;

    _boot_stage_sector[_boot_stage_out.fill++] = value;
    _boot_stage_out.written++;
    if (_boot_stage_out.fill == FLASH_SECTOR_SIZE)
        _boot_stage_out_flush();
    return true;
}

// --- Stored ---

static bool _boot_stage_copy(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (!_boot_stage_out_put(data[i]))
            return false;
    }
    return true;
}

// --- Raw deflate (RFC 1951) ---

// Canonical Huffman decoding a bit at a time, after Mark Adler's puff: slow beside a table decoder,
// but small, and an update is inflated once
#define BOOT_INFLATE_MAX_BITS   15u
#define BOOT_INFLATE_MAX_LCODES 286u
#define BOOT_INFLATE_MAX_DCODES 30u
#define BOOT_INFLATE_FIX_LCODES 288u
#define BOOT_INFLATE_WINDOW_LEN 32768u

typedef struct
{
    uint16_t count[BOOT_INFLATE_MAX_BITS + 1u];
    uint16_t symbol[BOOT_INFLATE_FIX_LCODES];
} boot_huffman_s;

typedef struct
{
    const uint8_t *in;
    uint32_t in_len;
    uint32_t in_at;
    uint32_t bit_buf;
    uint32_t bit_count;
    bool     failed;
} boot_inflate_s;

static boot_inflate_s _boot_inflate;
static boot_huffman_s _boot_lencode;
static boot_huffman_s _boot_distcode;
// Back references reach up to 32 KB behind: the output, kept here as well as written out
static uint8_t _boot_window[BOOT_INFLATE_WINDOW_LEN];

static const uint16_t _boot_len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t _boot_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t _boot_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t _boot_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static uint32_t _boot_inflate_bits(uint32_t need)
{
    uint32_t value = _boot_inflate.bit_buf;

    while (_boot_inflate.bit_count < need)
    {
        if (_boot_inflate.in_at >= _boot_inflate.in_len)
        {
            _boot_inflate.failed = true;
            return 0;
        }
        value |= (uint32_t)_boot_inflate.in[_boot_inflate.in_at++] << _boot_inflate.bit_count;
        _boot_inflate.bit_count += 8u;
    }

    _boot_inflate.bit_buf = value >> need;
    _boot_inflate.bit_count -= need;
    return value & ((1u << need) - 1u);
}

static bool _boot_inflate_emit(uint8_t value)
{
    _boot_window[_boot_stage_out.written % BOOT_INFLATE_WINDOW_LEN] = value;
    return _boot_stage_out_put(value);
}

static int32_t _boot_huffman_decode(const boot_huffman_s *h)
{
    int32_t code = 0;
    int32_t first = 0;
    int32_t index = 0;

    for (uint32_t len = 1; len <= BOOT_INFLATE_MAX_BITS; len++)
    {
        code |= (int32_t)_boot_inflate_bits(1);
        int32_t count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// False for an over-subscribed set of lengths; an incomplete one is allowed (a single distance code)
static bool _boot_huffman_build(boot_huffman_s *h, const uint8_t *lengths, uint32_t n)
{
    uint16_t offs[BOOT_INFLATE_MAX_BITS + 1u];
    int32_t left = 1;

    memset(h->count, 0, sizeof(h->count));
    for (uint32_t s = 0; s < n; s++)
        h->count[lengths[s]]++;

    for (uint32_t len = 1; len <= BOOT_INFLATE_MAX_BITS; len++)
    {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return false;
    }

    offs[1] = 0;
    for (uint32_t len = 1; len < BOOT_INFLATE_MAX_BITS; len++)
        offs[len + 1u] = (uint16_t)(offs[len] + h->count[len]);

    for (uint32_t s = 0; s < n; s++)
    {
        if (lengths[s])
            h->symbol[offs[lengths[s]]++] = (uint16_t)s;
    }
    return true;
}

static bool _boot_inflate_stored(void)
{
    // To the byte boundary, then a length and its complement
    _boot_inflate.bit_buf = 0;
    _boot_inflate.bit_count = 0;

    if (_boot_inflate.in_at + 4u > _boot_inflate.in_len)
        return false;

    const uint8_t *at = &_boot_inflate.in[_boot_inflate.in_at];
    uint32_t len = at[0] | ((uint32_t)at[1] << 8);
    uint32_t nlen = at[2] | ((uint32_t)at[3] << 8);
    _boot_inflate.in_at += 4u;

    if ((len != (~nlen & 0xFFFFu)) || (_boot_inflate.in_at + len > _boot_inflate.in_len))
        return false;

    while (len--)
    {
        if (!_boot_inflate_emit(_boot_inflate.in[_boot_inflate.in_at++]))
            return false;
    }
    return true;
}

static bool _boot_inflate_codes(void)
{
    for (;;)
    {
        int32_t symbol = _boot_huffman_decode(&_boot_lencode);
        if ((symbol < 0) || _boot_inflate.failed)
            return false;

        if (symbol < 256)
        {
            if (!_boot_inflate_emit((uint8_t)symbol))
                return false;
            continue;
        }

        if (symbol == 256)
            return true;

        symbol -= 257;
        if (symbol >= 29)
            return false;
        uint32_t len = _boot_len_base[symbol] + _boot_inflate_bits(_boot_len_extra[symbol]);

        symbol = _boot_huffman_decode(&_boot_distcode);
        if ((symbol < 0) || (symbol >= 30))
            return false;
        uint32_t dist = _boot_dist_base[symbol] + _boot_inflate_bits(_boot_dist_extra[symbol]);

        if (_boot_inflate.failed || (dist > _boot_stage_out.written) || (dist > BOOT_INFLATE_WINDOW_LEN))
            return false;

        while (len--)
        {
            uint8_t value = _boot_window[(_boot_stage_out.written - dist) % BOOT_INFLATE_WINDOW_LEN];
            if (!_boot_inflate_emit(value))
                return false;
        }
    }
}

static bool _boot_inflate_fixed(void)
{
    static uint8_t lengths[BOOT_INFLATE_FIX_LCODES];
    uint32_t s = 0;

    for (; s < 144u; s++)
        lengths[s] = 8;
    for (; s < 256u; s++)
        lengths[s] = 9;
    for (; s < 280u; s++)
        lengths[s] = 7;
    for (; s < BOOT_INFLATE_FIX_LCODES; s++)
        lengths[s] = 8;
    _boot_huffman_build(&_boot_lencode, lengths, BOOT_INFLATE_FIX_LCODES);

    for (s = 0; s < BOOT_INFLATE_MAX_DCODES; s++)
        lengths[s] = 5;
    _boot_huffman_build(&_boot_distcode, lengths, BOOT_INFLATE_MAX_DCODES);

    return _boot_inflate_codes();
}

static bool _boot_inflate_dynamic(void)
{
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    static uint8_t lengths[BOOT_INFLATE_MAX_LCODES + BOOT_INFLATE_MAX_DCODES];

    uint32_t nlen = _boot_inflate_bits(5) + 257u;
    uint32_t ndist = _boot_inflate_bits(5) + 1u;
    uint32_t ncode = _boot_inflate_bits(4) + 4u;
    if ((nlen > BOOT_INFLATE_MAX_LCODES) || (ndist > BOOT_INFLATE_MAX_DCODES) || _boot_inflate.failed)
        return false;

    // The code lengths' own code
    memset(lengths, 0, 19);
    for (uint32_t i = 0; i < ncode; i++)
        lengths[order[i]] = (uint8_t)_boot_inflate_bits(3);
    if (!_boot_huffman_build(&_boot_lencode, lengths, 19))
        return false;

    for (uint32_t i = 0; i < nlen + ndist;)
    {
        int32_t symbol = _boot_huffman_decode(&_boot_lencode);
        if ((symbol < 0) || _boot_inflate.failed)
            return false;

        if (symbol < 16)
        {
            lengths[i++] = (uint8_t)symbol;
            continue;
        }

        uint8_t repeat_len = 0;
        uint32_t repeat;
        if (symbol == 16)
        {
            if (i == 0)
                return false;
            repeat_len = lengths[i - 1u];
            repeat = 3u + _boot_inflate_bits(2);
        }
        else if (symbol == 17)
        {
            repeat = 3u + _boot_inflate_bits(3);
        }
        else
        {
            repeat = 11u + _boot_inflate_bits(7);
        }

        if (i + repeat > nlen + ndist)
            return false;
        while (repeat--)
            lengths[i++] = repeat_len;
    }

    // A block must be able to end
    if (lengths[256] == 0)
        return false;

    if (!_boot_huffman_build(&_boot_lencode, lengths, nlen) ||
        !_boot_huffman_build(&_boot_distcode, &lengths[nlen], ndist))
        return false;

    return _boot_inflate_codes();
}

static bool _boot_stage_inflate(const uint8_t *data, uint32_t len)
{
    memset(&_boot_inflate, 0, sizeof(_boot_inflate));
    _boot_inflate.in = data;
    _boot_inflate.in_len = len;

    uint32_t last;
    do
    {
        last = _boot_inflate_bits(1);
        uint32_t type = _boot_inflate_bits(2);
        bool ok;

        switch (type)
        {
            case 0:
                ok = _boot_inflate_stored();
                break;

            case 1:
                ok = _boot_inflate_fixed();
                break;

            case 2:
                ok = _boot_inflate_dynamic();
                break;

            default:
                ok = false;
                break;
        }

        if (!ok || _boot_inflate.failed)
            return false;
    } while (!last);

    return true;
}

// --- Apply ---

static bool _boot_stage_header_whole(const boot_stage_header_s *header)
{
    return (header->magic == BOOT_STAGE_MAGIC) && (header->magic_inv == ~BOOT_STAGE_MAGIC) &&
           (header->version == BOOT_STAGE_VERSION);
}

// Where the image goes, if it fits there and the data it comes from lies wholly above it
static bool _boot_stage_destination(const boot_stage_header_s *header, uint32_t *offset)
{
    uint32_t room;

    switch (header->destination)
    {
        case BOOT_STAGE_TO_APP:
            *offset = BOOT_LAYOUT_APP_OFFSET;
            room = BOOT_LAYOUT_APP_MAX;
            break;

        case BOOT_STAGE_TO_RADIO:
            *offset = BOOT_LAYOUT_RADIO_OFFSET;
            room = BOOT_LAYOUT_RADIO_SIZE;
            break;

        default:
            return false;
    }

    if ((header->image_size == 0) || (header->image_size > room))
        return false;

    if ((header->staged_size == 0) || (header->staged_size > BOOT_LAYOUT_APP_MAX))
        return false;

    if (BOOT_STAGE_DATA_OFFSET(header->staged_size) < *offset + header->image_size)
        return false;

    if (header->encoding == BOOT_STAGE_STORED)
        return header->staged_size == header->image_size;

    return header->encoding == BOOT_STAGE_DEFLATE;
}

void boot_stage_apply(void)
{
    const boot_stage_header_s *header = (const boot_stage_header_s *)(XIP_BASE + BOOT_LAYOUT_STAGE_HEADER_OFFSET);

    if (!_boot_stage_header_whole(header))
        return;

    uint32_t offset;
    const uint8_t *data = (const uint8_t *)(XIP_BASE + BOOT_STAGE_DATA_OFFSET(header->staged_size));
    boot_rgb_task(BOOT_RGB_WRITING);

    if (_boot_stage_destination(header, &offset) &&
        (boot_flash_crc32(data, header->staged_size) == header->staged_crc32))
    {
        memset(&_boot_stage_out, 0, sizeof(_boot_stage_out));
        _boot_stage_out.offset = offset;
        _boot_stage_out.limit = header->image_size;

        bool ok = (header->encoding == BOOT_STAGE_DEFLATE) ? _boot_stage_inflate(data, header->staged_size)
                                                           : _boot_stage_copy(data, header->staged_size);
        _boot_stage_out_flush();

        // A result short of its size or unlike its CRC must not run: without its first sector the
        // firmware is invalid (the update mode takes over), and the radio's header unmatched
        ok = ok && (_boot_stage_out.written == header->image_size) &&
             (boot_flash_crc32((const uint8_t *)(XIP_BASE + offset), header->image_size) == header->image_crc32);
        if (!ok)
            boot_flash_erase(offset, FLASH_SECTOR_SIZE);
    }

    // Written, or not worth writing: either way it is done with
    boot_flash_erase(BOOT_LAYOUT_STAGE_HEADER_OFFSET, FLASH_SECTOR_SIZE);
}
