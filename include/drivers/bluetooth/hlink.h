#ifndef DRIVERS_BLUETOOTH_HLINK_H
#define DRIVERS_BLUETOOTH_HLINK_H

// HOJA link: packets between the RP2040 and the ESP32 HCI bridge over I2C.
//
// The ESP32 slave can only answer a read with a frame it loaded beforehand (32 byte FIFO, no
// clock stretching), so every transfer is one fixed size frame with a CRC, a sequence number and
// an ack. Bad or stale reads just get dropped and the sender repeats from the oldest unacked
// frame (go-back-N, up to HLINK_WINDOW in flight).
//
// Each side sends a session byte picked at boot and echoes the peer's. A changed session means
// the peer restarted: link state is reset and rx_cb gets HLINK_CH_IDLE.
//
// Both firmwares build this same file. Not thread safe, callers lock around it.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HLINK_FRAME_SIZE    32
#define HLINK_PAYLOAD_MAX   25
#define HLINK_WINDOW        8    // Frames in flight (4 bit sequence numbers)
#define HLINK_PACKET_MAX    1100 // Largest packet (HCI ACL 1021 + header)

// Bump when frames or control messages change
#define HLINK_PROTOCOL_VERSION 1

typedef enum
{
    HLINK_CH_IDLE    = 0, // Ack only
    HLINK_CH_HCI_CMD = 1, // HCI channels match their H4 packet type
    HLINK_CH_HCI_ACL = 2,
    HLINK_CH_HCI_SCO = 3,
    HLINK_CH_HCI_EVT = 4,
    HLINK_CH_CTRL    = 8, // hlink_ctrl_t messages
    HLINK_CH_LOG     = 9, // Text printed on the ESP32 console
} hlink_channel_t;

// Control messages: byte 0 is the message. RP2040 to ESP32 below 0x80, ESP32 to RP2040 above.
typedef enum
{
    HLINK_CTRL_HELLO        = 0x01, // [protocol]
    HLINK_CTRL_START_RADIO  = 0x02, // [bd_addr x6] Start the controller with this address
    HLINK_CTRL_BATMON       = 0x03, // [enable, adc gpio]
    HLINK_CTRL_PING         = 0x04, // Nothing, only the link's ack matters

    HLINK_CTRL_HELLO_RSP    = 0x81, // [protocol, fw version hi, lo]
    HLINK_CTRL_RADIO_READY  = 0x82, // [status] 0 when the controller is up
    HLINK_CTRL_BATTERY      = 0x83, // [mv hi, lo]
    HLINK_CTRL_CONSOLE      = 0x84, // [text] A line typed on the ESP32 console
} hlink_ctrl_t;

// Return false when there is no room, the frame stays unacked and gets repeated.
// HLINK_CH_IDLE (peer restarted) can't be refused.
typedef bool (*hlink_rx_cb_t)(void *ctx, uint8_t channel, const uint8_t *data, uint16_t len);

typedef struct
{
    uint32_t frames_tx;
    uint32_t frames_rx;
    uint32_t crc_errors;
    uint32_t out_of_order;    // Repeats, or frames past a gap
    uint32_t retransmits;
    uint32_t packets_tx;
    uint32_t packets_rx;
    uint32_t packets_dropped; // Torn or oversized packets
    uint32_t peer_restarts;
    uint32_t refused;
} hlink_stats_s;

typedef struct
{
    // Outgoing packets, [channel][len lo][len hi][data]
    uint8_t  *txq;
    uint32_t  txq_size;
    uint32_t  txq_head;
    uint32_t  txq_tail;
    uint32_t  txq_used;
    uint16_t  txq_pkt_left; // Bytes of the current packet still to frame
    uint8_t   txq_pkt_ch;

    // Unacked frames, by sequence % HLINK_WINDOW
    uint8_t   frames[HLINK_WINDOW][HLINK_FRAME_SIZE];
    uint8_t   seq_base; // Oldest unacked
    uint8_t   seq_head; // Next to assign
    uint8_t   seq_next; // Next to send
    uint8_t   stall;    // Frames sent since the ack last moved

    uint8_t   rx_expected;
    uint8_t   rx_ch;
    uint16_t  rx_len;
    uint16_t  rx_total;
    bool      rx_active;
    bool      nak_pending;
    bool      nak_sent;
    uint8_t   rx_buf[HLINK_PACKET_MAX];

    bool      peer_seen;
    uint8_t   session;      // Ours, never 0
    uint8_t   peer_session; // 0 until heard
    uint8_t   new_session;  // A new session must show up twice

    hlink_rx_cb_t rx_cb;
    void         *rx_ctx;

    hlink_stats_s stats;
} hlink_s;

// txq is the caller's storage for outgoing packets. session should be random per boot.
void hlink_init(hlink_s *link, uint8_t *txq, uint32_t txq_size, uint8_t session,
                hlink_rx_cb_t rx_cb, void *rx_ctx);
void hlink_reset(hlink_s *link);

// False (and nothing queued) if the whole packet doesn't fit
bool hlink_send(hlink_s *link, uint8_t channel, const uint8_t *data, uint16_t len);
bool hlink_send2(hlink_s *link, uint8_t channel, const uint8_t *hdr, uint16_t hdr_len,
                 const uint8_t *body, uint16_t body_len);

uint32_t hlink_tx_free(const hlink_s *link);
static inline uint32_t hlink_packet_cost(uint16_t len) { return (uint32_t)len + 3; }
bool hlink_tx_pending(const hlink_s *link);

// Next frame to send, always HLINK_FRAME_SIZE bytes
void hlink_build(hlink_s *link, uint8_t out[HLINK_FRAME_SIZE]);

// False if the frame failed its CRC
bool hlink_receive(hlink_s *link, const uint8_t in[HLINK_FRAME_SIZE]);

uint16_t hlink_crc16(const uint8_t *data, size_t len);

#endif
