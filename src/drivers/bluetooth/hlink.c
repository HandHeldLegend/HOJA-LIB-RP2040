#include "drivers/bluetooth/hlink.h"

#include <string.h>

// Frame layout
//  [0]      sequence (low nibble) | acknowledgement: next sequence expected (high nibble)
//  [1]      channel (bits 0-4) | FIRST (bit 5) | NAK (bit 6) | DATA (bit 7)
//  [2]      payload length
//  [3..27]  payload. A packet's FIRST frame starts with its length (2 bytes, little endian).
//  [28]     receiver's session as the sender last heard it
//  [29]     sender's session
//  [30..31] CRC-16 of bytes 0..29, big endian
#define HLINK_IDX_SEQ   0
#define HLINK_IDX_FLAGS 1
#define HLINK_IDX_LEN   2
#define HLINK_IDX_DATA  3
#define HLINK_IDX_ECHO  28
#define HLINK_IDX_SESSION 29
#define HLINK_IDX_CRC   30

#define HLINK_FLAG_FIRST 0x20
#define HLINK_FLAG_NAK   0x40
#define HLINK_FLAG_DATA  0x80
#define HLINK_CH_MASK    0x1F

#define HLINK_SEQ_MASK   0x0F

// Frames sent without the ack moving before resending (an ack takes ~2 exchanges)
#define HLINK_RETX_STALL (HLINK_WINDOW + 4)

static const uint16_t _crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
};

uint16_t hlink_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++)
        crc = (uint16_t)((crc << 8) ^ _crc_table[((crc >> 8) ^ data[i]) & 0xFF]);
    return crc;
}

static inline uint8_t _in_flight(const hlink_s *link)
{
    return (uint8_t)((link->seq_head - link->seq_base) & HLINK_SEQ_MASK);
}

void hlink_reset(hlink_s *link)
{
    link->txq_head = link->txq_tail = link->txq_used = 0;
    link->txq_pkt_left = 0;
    link->seq_base = link->seq_head = link->seq_next = 0;
    link->stall = 0;
    link->rx_expected = 0;
    link->rx_active = false;
    link->nak_pending = false;
    link->nak_sent = false;
    link->rx_len = 0;
    link->rx_total = 0;
    link->peer_seen = false;
}

void hlink_init(hlink_s *link, uint8_t *txq, uint32_t txq_size, uint8_t session,
                hlink_rx_cb_t rx_cb, void *rx_ctx)
{
    memset(link, 0, sizeof(*link));
    link->session = session ? session : 1;
    link->txq = txq;
    link->txq_size = txq_size;
    link->rx_cb = rx_cb;
    link->rx_ctx = rx_ctx;
    hlink_reset(link);
}

uint32_t hlink_tx_free(const hlink_s *link)
{
    return link->txq_size - link->txq_used;
}

bool hlink_tx_pending(const hlink_s *link)
{
    return link->txq_used || link->txq_pkt_left || _in_flight(link);
}

static void _txq_put(hlink_s *link, const uint8_t *data, uint32_t len)
{
    while(len)
    {
        uint32_t chunk = link->txq_size - link->txq_head;
        if(chunk > len)
            chunk = len;
        memcpy(&link->txq[link->txq_head], data, chunk);
        link->txq_head = (link->txq_head + chunk) % link->txq_size;
        link->txq_used += chunk;
        data += chunk;
        len -= chunk;
    }
}

static void _txq_get(hlink_s *link, uint8_t *out, uint32_t len)
{
    while(len)
    {
        uint32_t chunk = link->txq_size - link->txq_tail;
        if(chunk > len)
            chunk = len;
        memcpy(out, &link->txq[link->txq_tail], chunk);
        link->txq_tail = (link->txq_tail + chunk) % link->txq_size;
        link->txq_used -= chunk;
        out += chunk;
        len -= chunk;
    }
}

bool hlink_send2(hlink_s *link, uint8_t channel, const uint8_t *hdr, uint16_t hdr_len,
                 const uint8_t *body, uint16_t body_len)
{
    uint32_t total = (uint32_t)hdr_len + body_len;
    if(total > HLINK_PACKET_MAX || channel == HLINK_CH_IDLE || channel > HLINK_CH_MASK)
        return false;
    if(hlink_tx_free(link) < hlink_packet_cost((uint16_t)total))
        return false;

    uint8_t head[3] = {channel, (uint8_t)(total & 0xFF), (uint8_t)(total >> 8)};
    _txq_put(link, head, 3);
    if(hdr_len)
        _txq_put(link, hdr, hdr_len);
    if(body_len)
        _txq_put(link, body, body_len);
    link->stats.packets_tx++;
    return true;
}

bool hlink_send(hlink_s *link, uint8_t channel, const uint8_t *data, uint16_t len)
{
    return hlink_send2(link, channel, data, len, NULL, 0);
}

static inline void _stamp(const hlink_s *link, uint8_t *frame, bool nak)
{
    frame[HLINK_IDX_SEQ] = (uint8_t)((frame[HLINK_IDX_SEQ] & HLINK_SEQ_MASK) | (link->rx_expected << 4));
    if(nak)
        frame[HLINK_IDX_FLAGS] |= HLINK_FLAG_NAK;
    else
        frame[HLINK_IDX_FLAGS] &= (uint8_t)~HLINK_FLAG_NAK;
    frame[HLINK_IDX_ECHO] = link->peer_session;
    frame[HLINK_IDX_SESSION] = link->session;
    uint16_t crc = hlink_crc16(frame, HLINK_IDX_CRC);
    frame[HLINK_IDX_CRC] = (uint8_t)(crc >> 8);
    frame[HLINK_IDX_CRC + 1] = (uint8_t)(crc & 0xFF);
}

// Cut the next frame's worth of queued data into frames[seq_head]
static bool _frame_new(hlink_s *link)
{
    if(!link->txq_pkt_left && !link->txq_used)
        return false;

    uint8_t *f = link->frames[link->seq_head % HLINK_WINDOW];
    memset(f, 0, HLINK_FRAME_SIZE);

    uint8_t flags = HLINK_FLAG_DATA;
    uint8_t len = 0;
    uint8_t *p = &f[HLINK_IDX_DATA];

    if(!link->txq_pkt_left)
    {
        // Start the packet at the tail of the queue
        uint8_t head[3];
        _txq_get(link, head, 3);
        link->txq_pkt_ch = head[0];
        link->txq_pkt_left = (uint16_t)(head[1] | (head[2] << 8));
        flags |= HLINK_FLAG_FIRST;
        p[0] = head[1];
        p[1] = head[2];
        len = 2;
    }

    uint16_t chunk = HLINK_PAYLOAD_MAX - len;
    if(chunk > link->txq_pkt_left)
        chunk = link->txq_pkt_left;
    _txq_get(link, &p[len], chunk);
    link->txq_pkt_left -= chunk;
    len += (uint8_t)chunk;

    f[HLINK_IDX_SEQ] = link->seq_head & HLINK_SEQ_MASK;
    f[HLINK_IDX_FLAGS] = (uint8_t)(flags | link->txq_pkt_ch);
    f[HLINK_IDX_LEN] = len;

    link->seq_head = (link->seq_head + 1) & HLINK_SEQ_MASK;
    return true;
}

void hlink_build(hlink_s *link, uint8_t out[HLINK_FRAME_SIZE])
{
    bool nak = link->nak_pending;
    link->nak_pending = false;

    if(_in_flight(link))
    {
        if(++link->stall >= HLINK_RETX_STALL)
        {
            // Acks stopped, resend from the oldest frame
            link->seq_next = link->seq_base;
            link->stall = 0;
            link->stats.retransmits++;
        }
    }

    // New data only once nothing is waiting to be resent
    if(link->seq_next == link->seq_head && _in_flight(link) < HLINK_WINDOW)
        _frame_new(link);

    if(link->seq_next != link->seq_head)
    {
        memcpy(out, link->frames[link->seq_next % HLINK_WINDOW], HLINK_FRAME_SIZE);
        link->seq_next = (link->seq_next + 1) & HLINK_SEQ_MASK;
    }
    else
    {
        memset(out, 0, HLINK_FRAME_SIZE);
        out[HLINK_IDX_FLAGS] = HLINK_CH_IDLE;
    }

    _stamp(link, out, nak);
    link->stats.frames_tx++;
}

// False when the application refused the packet, the frame then counts as not received
static bool _deliver(hlink_s *link, uint8_t ch, const uint8_t *data, uint8_t len, bool first)
{
    if(first)
    {
        if(link->rx_active)
            link->stats.packets_dropped++; // The previous packet never completed

        if(len < 2)
        {
            link->rx_active = false;
            link->stats.packets_dropped++;
            return true;
        }

        uint16_t total = (uint16_t)(data[0] | (data[1] << 8));
        data += 2;
        len -= 2;

        if(total > HLINK_PACKET_MAX)
        {
            link->rx_active = false;
            link->stats.packets_dropped++;
            return true;
        }

        link->rx_active = true;
        link->rx_ch = ch;
        link->rx_len = 0;
        link->rx_total = total;
    }
    else if(!link->rx_active || ch != link->rx_ch)
    {
        link->rx_active = false;
        link->stats.packets_dropped++;
        return true;
    }

    if((uint32_t)link->rx_len + len > link->rx_total)
    {
        link->rx_active = false;
        link->stats.packets_dropped++;
        return true;
    }

    memcpy(&link->rx_buf[link->rx_len], data, len);
    link->rx_len += len;

    if(link->rx_len == link->rx_total)
    {
        if(link->rx_cb && !link->rx_cb(link->rx_ctx, link->rx_ch, link->rx_buf, link->rx_len))
        {
            // Undo the frame, the sender repeats it
            link->rx_len -= len;
            if(first)
                link->rx_active = false;
            link->stats.refused++;
            return false;
        }
        link->rx_active = false;
        link->stats.packets_rx++;
    }
    return true;
}

bool hlink_receive(hlink_s *link, const uint8_t in[HLINK_FRAME_SIZE])
{
    uint16_t crc = (uint16_t)((in[HLINK_IDX_CRC] << 8) | in[HLINK_IDX_CRC + 1]);
    if(hlink_crc16(in, HLINK_IDX_CRC) != crc || in[HLINK_IDX_LEN] > HLINK_PAYLOAD_MAX)
    {
        link->stats.crc_errors++;
        return false;
    }

    link->stats.frames_rx++;

    uint8_t session = in[HLINK_IDX_SESSION];
    if(session != link->peer_session)
    {
        // A new session only counts on its second frame, a bad frame past the CRC isn't a reboot
        if(session != link->new_session)
        {
            link->new_session = session;
            return true;
        }
        bool restarted = link->peer_session != 0;
        link->new_session = 0;
        link->peer_session = session;
        if(restarted)
        {
            // Peer rebooted, drop everything from before
            hlink_reset(link);
            link->stats.peer_restarts++;
            if(link->rx_cb)
                link->rx_cb(link->rx_ctx, HLINK_CH_IDLE, NULL, 0);
        }
    }
    link->peer_seen = true;

    // Sent before the peer knew our session
    if(in[HLINK_IDX_ECHO] != link->session)
        return true;

    uint8_t flags = in[HLINK_IDX_FLAGS];

    // Everything before ack arrived
    uint8_t ack = in[HLINK_IDX_SEQ] >> 4;
    uint8_t acked = (uint8_t)((ack - link->seq_base) & HLINK_SEQ_MASK);
    if(acked && acked <= _in_flight(link))
    {
        link->seq_base = ack;
        link->stall = 0;
        // A rewind may have left seq_next behind the ack
        if(((link->seq_next - link->seq_base) & HLINK_SEQ_MASK) > _in_flight(link))
            link->seq_next = link->seq_base;
    }
    else if((flags & HLINK_FLAG_NAK) && _in_flight(link) && link->seq_next != link->seq_base)
    {
        // Peer saw a gap, resend from the oldest unacked frame now
        link->seq_next = link->seq_base;
        link->stall = 0;
        link->stats.retransmits++;
    }

    if(!(flags & HLINK_FLAG_DATA))
        return true;

    uint8_t seq = in[HLINK_IDX_SEQ] & HLINK_SEQ_MASK;
    if(seq != link->rx_expected)
    {
        link->stats.out_of_order++;
        // Ahead of sequence means one went missing (a repeat doesn't)
        uint8_t ahead = (uint8_t)((seq - link->rx_expected) & HLINK_SEQ_MASK);
        if(ahead && ahead < HLINK_WINDOW && !link->nak_sent)
        {
            link->nak_pending = true;
            link->nak_sent = true;
        }
        return true;
    }

    if(_deliver(link, flags & HLINK_CH_MASK, &in[HLINK_IDX_DATA], in[HLINK_IDX_LEN],
                 (flags & HLINK_FLAG_FIRST) != 0))
    {
        link->rx_expected = (link->rx_expected + 1) & HLINK_SEQ_MASK;
        link->nak_sent = false;
    }
    return true;
}
