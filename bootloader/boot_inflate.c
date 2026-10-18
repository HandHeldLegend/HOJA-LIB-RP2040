#include "boot_inflate.h"

#include <string.h>

// Canonical Huffman decoding a bit at a time, after Mark Adler's puff
// (https://github.com/madler/zlib/blob/master/contrib/puff/puff.c): slow beside a table decoder,
// but small, and an update is inflated once. Section numbers below are RFC 1951's.

#define BOOT_INFLATE_MAX_BITS     15u    // The longest code (3.2.7)
#define BOOT_INFLATE_MAX_LCODES   286u   // Literal/length codes a dynamic block may have (3.2.7)
#define BOOT_INFLATE_MAX_DCODES   30u    // Distance codes (3.2.5)
#define BOOT_INFLATE_FIX_LCODES   288u   // Literal/length codes in the fixed code (3.2.6)
#define BOOT_INFLATE_LEN_CODES    29u    // Length symbols, 257 to 285 (3.2.5)
#define BOOT_INFLATE_CLEN_CODES   19u    // Code length codes (3.2.7)
#define BOOT_INFLATE_WINDOW_LEN   32768u // The furthest a distance reaches back (3.2.5)

// Literal/length symbols: literal bytes below the end of a block, lengths above it (3.2.5)
#define BOOT_INFLATE_END_OF_BLOCK 256
#define BOOT_INFLATE_LEN_FIRST    257

// The block types in a block's header (3.2.3)
#define BOOT_INFLATE_BLOCK_STORED  0u
#define BOOT_INFLATE_BLOCK_FIXED   1u
#define BOOT_INFLATE_BLOCK_DYNAMIC 2u

// The code length code's symbols that repeat (3.2.7): the previous length 3 to 6 times, a zero
// length 3 to 10 times, a zero length 11 to 138 times
#define BOOT_INFLATE_REPEAT_PREV   16
#define BOOT_INFLATE_REPEAT_ZEROS  17

typedef struct
{
    uint16_t count[BOOT_INFLATE_MAX_BITS + 1u]; // Codes of each length
    uint16_t symbol[BOOT_INFLATE_FIX_LCODES];   // Symbols in code order
} boot_inflate_huffman_s;

typedef struct
{
    const uint8_t *in;
    uint32_t in_len;
    uint32_t in_at;
    uint32_t bit_buf;
    uint32_t bit_count;
    uint32_t out_len;           // Bytes put so far
    bool (*put)(uint8_t value);
    bool     failed;            // The input ran out part way through a field
} boot_inflate_state_s;

static boot_inflate_state_s _boot_inflate;
static boot_inflate_huffman_s _boot_inflate_lencode;
static boot_inflate_huffman_s _boot_inflate_distcode;
// Back references reach up to 32 KB behind: the output, kept here as well as put
static uint8_t _boot_inflate_window[BOOT_INFLATE_WINDOW_LEN];

// Each length and distance symbol's base and extra bits (3.2.5)
static const uint16_t _boot_inflate_len_base[BOOT_INFLATE_LEN_CODES] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t _boot_inflate_len_extra[BOOT_INFLATE_LEN_CODES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t _boot_inflate_dist_base[BOOT_INFLATE_MAX_DCODES] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t _boot_inflate_dist_extra[BOOT_INFLATE_MAX_DCODES] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// The next need bits, first bit lowest (3.1.1). Past the input's end it sets failed and gives 0.
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
    _boot_inflate_window[_boot_inflate.out_len % BOOT_INFLATE_WINDOW_LEN] = value;
    _boot_inflate.out_len++;
    return _boot_inflate.put(value);
}

static int32_t _boot_inflate_decode(const boot_inflate_huffman_s *h)
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
static bool _boot_inflate_build(boot_inflate_huffman_s *h, const uint8_t *lengths, uint32_t n)
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

// A stored block (3.2.4): to the byte boundary, then LEN, its complement NLEN, and LEN bytes
static bool _boot_inflate_stored(void)
{
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

// A Huffman block's data, to its end of block symbol (3.2.5)
static bool _boot_inflate_codes(void)
{
    for (;;)
    {
        int32_t symbol = _boot_inflate_decode(&_boot_inflate_lencode);
        if ((symbol < 0) || _boot_inflate.failed)
            return false;

        if (symbol < BOOT_INFLATE_END_OF_BLOCK)
        {
            if (!_boot_inflate_emit((uint8_t)symbol))
                return false;
            continue;
        }

        if (symbol == BOOT_INFLATE_END_OF_BLOCK)
            return true;

        symbol -= BOOT_INFLATE_LEN_FIRST;
        if (symbol >= (int32_t)BOOT_INFLATE_LEN_CODES)
            return false;
        uint32_t len = _boot_inflate_len_base[symbol];
        len += _boot_inflate_bits(_boot_inflate_len_extra[symbol]);

        symbol = _boot_inflate_decode(&_boot_inflate_distcode);
        if ((symbol < 0) || (symbol >= (int32_t)BOOT_INFLATE_MAX_DCODES))
            return false;
        uint32_t dist = _boot_inflate_dist_base[symbol];
        dist += _boot_inflate_bits(_boot_inflate_dist_extra[symbol]);

        if (_boot_inflate.failed || (dist > _boot_inflate.out_len) || (dist > BOOT_INFLATE_WINDOW_LEN))
            return false;

        while (len--)
        {
            uint8_t value = _boot_inflate_window[(_boot_inflate.out_len - dist) % BOOT_INFLATE_WINDOW_LEN];
            if (!_boot_inflate_emit(value))
                return false;
        }
    }
}

// A block in the fixed code (3.2.6): literal/length codes 0-143 of 8 bits, 144-255 of 9, 256-279
// of 7 and 280-287 of 8; distance codes all of 5
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
    _boot_inflate_build(&_boot_inflate_lencode, lengths, BOOT_INFLATE_FIX_LCODES);

    for (s = 0; s < BOOT_INFLATE_MAX_DCODES; s++)
        lengths[s] = 5;
    _boot_inflate_build(&_boot_inflate_distcode, lengths, BOOT_INFLATE_MAX_DCODES);

    return _boot_inflate_codes();
}

// A block in its own code (3.2.7): the code's lengths, themselves in a code, then the data
static bool _boot_inflate_dynamic(void)
{
    // The order the code length code's lengths come in
    static const uint8_t order[BOOT_INFLATE_CLEN_CODES] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    static uint8_t lengths[BOOT_INFLATE_MAX_LCODES + BOOT_INFLATE_MAX_DCODES];

    // HLIT, HDIST and HCLEN: the counts of each code's lengths
    uint32_t nlen = _boot_inflate_bits(5) + BOOT_INFLATE_LEN_FIRST;
    uint32_t ndist = _boot_inflate_bits(5) + 1u;
    uint32_t ncode = _boot_inflate_bits(4) + 4u;
    if ((nlen > BOOT_INFLATE_MAX_LCODES) || (ndist > BOOT_INFLATE_MAX_DCODES) || _boot_inflate.failed)
        return false;

    // The code length code, 3 bits a length
    memset(lengths, 0, BOOT_INFLATE_CLEN_CODES);
    for (uint32_t i = 0; i < ncode; i++)
        lengths[order[i]] = (uint8_t)_boot_inflate_bits(3);
    if (!_boot_inflate_build(&_boot_inflate_lencode, lengths, BOOT_INFLATE_CLEN_CODES))
        return false;

    for (uint32_t i = 0; i < nlen + ndist;)
    {
        int32_t symbol = _boot_inflate_decode(&_boot_inflate_lencode);
        if ((symbol < 0) || _boot_inflate.failed)
            return false;

        if (symbol < BOOT_INFLATE_REPEAT_PREV)
        {
            lengths[i++] = (uint8_t)symbol;
            continue;
        }

        uint8_t repeat_len = 0;
        uint32_t repeat;
        if (symbol == BOOT_INFLATE_REPEAT_PREV)
        {
            if (i == 0)
                return false;
            repeat_len = lengths[i - 1u];
            repeat = 3u + _boot_inflate_bits(2);
        }
        else if (symbol == BOOT_INFLATE_REPEAT_ZEROS)
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
    if (lengths[BOOT_INFLATE_END_OF_BLOCK] == 0)
        return false;

    if (!_boot_inflate_build(&_boot_inflate_lencode, lengths, nlen) ||
        !_boot_inflate_build(&_boot_inflate_distcode, &lengths[nlen], ndist))
        return false;

    return _boot_inflate_codes();
}

bool boot_inflate(const uint8_t *in, uint32_t len, bool (*put)(uint8_t value))
{
    memset(&_boot_inflate, 0, sizeof(_boot_inflate));
    _boot_inflate.in = in;
    _boot_inflate.in_len = len;
    _boot_inflate.put = put;

    // Each block's header (3.2.3): BFINAL, then BTYPE
    uint32_t last;
    do
    {
        last = _boot_inflate_bits(1);
        uint32_t type = _boot_inflate_bits(2);
        bool ok;

        switch (type)
        {
            case BOOT_INFLATE_BLOCK_STORED:
                ok = _boot_inflate_stored();
                break;

            case BOOT_INFLATE_BLOCK_FIXED:
                ok = _boot_inflate_fixed();
                break;

            case BOOT_INFLATE_BLOCK_DYNAMIC:
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
