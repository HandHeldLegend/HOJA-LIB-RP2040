#include "input/snapback/snapback_capture.h"
#include "usb/webusb.h"

#include <stdbool.h>
#include <string.h>

// 256 samples at 2 kHz: 128 ms, from 24 ms before the stick falls through the release line until the
// ringing is over
#define SBCAP_SAMPLES       256u
#define SBCAP_PRE           48u
// A capture starts once the stick has been out past here and falls back through the release line:
// low enough to catch a partial tilt let go (a turnaround), not just releases from the edge
#define SBCAP_EDGE          800u
#define SBCAP_RELEASE       500u

// Per packet: id, stick, capture number, packet index, packet count, sample count, then x/y pairs
#define SBCAP_HEADER        6u
#define SBCAP_PER_PACKET    ((64u - SBCAP_HEADER) / 4u)
#define SBCAP_PACKETS       ((SBCAP_SAMPLES + SBCAP_PER_PACKET - 1u) / SBCAP_PER_PACKET)
// Samples between packets: sends from this core wait in a 4-deep queue the USB core empties
#define SBCAP_SEND_EVERY    4u

typedef enum
{
    SBCAP_WAIT_EDGE,
    SBCAP_AT_EDGE,
    SBCAP_RECORDING,
    SBCAP_SENDING,
} sbcap_state_t;

static volatile uint8_t _sbcap_stick = SNAPBACK_CAPTURE_OFF;
static uint8_t  _sbcap_armed_stick = SNAPBACK_CAPTURE_OFF;
static sbcap_state_t _sbcap_state = SBCAP_WAIT_EDGE;
static int16_t  _sbcap_ring[SBCAP_SAMPLES][2];
static uint16_t _sbcap_write = 0;
static uint16_t _sbcap_remaining = 0;
static uint16_t _sbcap_start = 0;
static uint8_t  _sbcap_number = 0;
static uint8_t  _sbcap_packet = 0;
static uint8_t  _sbcap_wait = 0;

void snapback_capture_arm(uint8_t stick)
{
    _sbcap_stick = (stick <= 1u) ? stick : SNAPBACK_CAPTURE_OFF;
}

static void _sbcap_send_packet(void)
{
    uint8_t out[64] = {0};
    const uint16_t first = (uint16_t)(_sbcap_packet * SBCAP_PER_PACKET);
    uint16_t count = (uint16_t)(SBCAP_SAMPLES - first);
    count = (count > SBCAP_PER_PACKET) ? SBCAP_PER_PACKET : count;

    out[0] = WEBUSB_SNAPBACK_CAPTURE;
    out[1] = _sbcap_armed_stick;
    out[2] = _sbcap_number;
    out[3] = _sbcap_packet;
    out[4] = (uint8_t)SBCAP_PACKETS;
    out[5] = (uint8_t)count;
    for (uint16_t i = 0; i < count; i++)
    {
        const uint16_t at = (uint16_t)((_sbcap_start + first + i) % SBCAP_SAMPLES);
        memcpy(&out[SBCAP_HEADER + i * 4u], &_sbcap_ring[at][0], 2);
        memcpy(&out[SBCAP_HEADER + i * 4u + 2u], &_sbcap_ring[at][1], 2);
    }
    webusb_send_bulk(out, (uint16_t)(SBCAP_HEADER + count * 4u));
}

void snapback_capture_process(const analog_axis_s *l, const analog_axis_s *r)
{
    const uint8_t stick = _sbcap_stick;
    if (stick == SNAPBACK_CAPTURE_OFF)
    {
        _sbcap_armed_stick = SNAPBACK_CAPTURE_OFF;
        return;
    }

    // Arming (or switching sticks) starts over
    if (stick != _sbcap_armed_stick)
    {
        _sbcap_armed_stick = stick;
        _sbcap_state = SBCAP_WAIT_EDGE;
    }

    if (_sbcap_state == SBCAP_SENDING)
    {
        if (++_sbcap_wait < SBCAP_SEND_EVERY)
            return;
        _sbcap_wait = 0;
        _sbcap_send_packet();
        if (++_sbcap_packet >= SBCAP_PACKETS)
        {
            _sbcap_number++;
            _sbcap_state = SBCAP_WAIT_EDGE;
        }
        return;
    }

    const analog_axis_s *a = (stick == 0u) ? l : r;
    _sbcap_ring[_sbcap_write][0] = a->x;
    _sbcap_ring[_sbcap_write][1] = a->y;
    _sbcap_write = (uint16_t)((_sbcap_write + 1u) % SBCAP_SAMPLES);

    switch (_sbcap_state)
    {
        case SBCAP_WAIT_EDGE:
            if (a->distance >= SBCAP_EDGE)
                _sbcap_state = SBCAP_AT_EDGE;
            break;

        case SBCAP_AT_EDGE:
            if (a->distance < SBCAP_RELEASE)
            {
                _sbcap_remaining = (uint16_t)(SBCAP_SAMPLES - SBCAP_PRE);
                _sbcap_state = SBCAP_RECORDING;
            }
            break;

        case SBCAP_RECORDING:
            if (--_sbcap_remaining == 0u)
            {
                // The ring is full from the oldest sample, which is the next one to be written
                _sbcap_start = _sbcap_write;
                _sbcap_packet = 0;
                _sbcap_wait = 0;
                _sbcap_state = SBCAP_SENDING;
            }
            break;

        default:
            break;
    }
}
