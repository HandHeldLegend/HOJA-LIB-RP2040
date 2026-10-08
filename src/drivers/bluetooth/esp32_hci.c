#include "board_config.h"

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER==BT_DRIVER_ESP32HCI)
#include "drivers/bluetooth/esp32_hci.h"
#include "drivers/bluetooth/hlink.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "hoja.h"
#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"
#include "hal/i2c_hal.h"
#include "transport/transport.h"
#include "transport/transport_bt.h"
#include "drivers/mux/pi3usb4000a.h"
#include "utilities/boot.h"
#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "btstack_config.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_transport.h"
#include "gap.h"

#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#define ESP32_HCI_I2C_ADDRESS 0x76

// Old baseband protocol, only used to read the firmware version
#define LEGACY_MSG_SIZE_OUT     32
#define LEGACY_MSG_SIZE_IN      24
#define LEGACY_CMD_FW_VERSION   0xFD

// The ESP32 slave needs the bus idle for a moment after a write before it answers a read
#define ENGINE_READ_GAP_US      20
// An exchange taking longer than a few exchange times failed
#define ENGINE_TIMEOUT_EXCHANGES 3
#define ENGINE_TIMEOUT_MIN_US   300
// Wait this long after a failure (the ESP32 may still be booting)
#define ENGINE_BACKOFF_US       1000
// In update mode the ESP32 belongs to esptool; poll it gently
#define ENGINE_UPDATE_PERIOD_US 5000

#define LINK_I2C (BLUETOOTH_DRIVER_I2C_INSTANCE ? i2c1 : i2c0)

static hlink_s _link;
static uint8_t _txq[4096];
static spin_lock_t *_lock;

// Received packets for the task loop or BTstack: [channel][len lo][len hi][data]
static uint8_t _rxq[4096];
static volatile uint32_t _rxq_head = 0;
static volatile uint32_t _rxq_tail = 0;

static core_params_s *_params = NULL;
static bool _altflash = false;
static uint16_t _esp32_version = 0;
static bool _esp32_version_read = false;
static volatile uint16_t _battery_mv = 0;

// Link engine: one exchange is a write of our frame, then a read of the ESP32's.
// DMA moves the bytes; the read's completion interrupt starts the next exchange.
static int _dma_tx = -1;
static int _dma_rx = -1;
static uint16_t _cmd[HLINK_FRAME_SIZE * 2];
static uint8_t _rx_frame[HLINK_FRAME_SIZE];
static volatile bool _busy = false;
static volatile bool _paused = false;
static volatile bool _reconfigure = true;
static volatile uint8_t _phase = 0; // 1: write, 2: read
static volatile uint64_t _busy_since_us = 0;
static volatile uint64_t _next_start_us = 0;
static uint32_t _timeout_us = 3000;

typedef struct
{
    uint32_t exchanges;
    uint32_t nacks;
    uint32_t timeouts;
} engine_stats_s;

static engine_stats_s _eng;

static void _esp32_enable(bool enabled)
{
    // EN has a pull-up, release it to run
    if(enabled)
    {
        gpio_hal_init(BLUETOOTH_DRIVER_ENABLE_PIN, false, true);
    }
    else
    {
        gpio_hal_init(BLUETOOTH_DRIVER_ENABLE_PIN, false, false);
        gpio_hal_write(BLUETOOTH_DRIVER_ENABLE_PIN, false);
    }
}

// USB to the ESP32's CH340 (true) or the RP2040 (false)
static void _esp32_usb_select(bool esp32)
{
    HOJA_USB_MUX_ENABLE(false);
    sys_hal_sleep_ms(20);
    HOJA_USB_MUX_SELECT(esp32 ? 1 : 0);
    sys_hal_sleep_ms(20);
    HOJA_USB_MUX_ENABLE(true);
}

static bool _link_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    uint32_t save = spin_lock_blocking(_lock);
    bool ok = hlink_send(&_link, channel, data, len);
    spin_unlock(_lock, save);
    return ok;
}

static uint32_t _link_tx_free()
{
    uint32_t save = spin_lock_blocking(_lock);
    uint32_t free = hlink_tx_free(&_link);
    spin_unlock(_lock, save);
    return free;
}

static uint32_t _rxq_used()
{
    return _rxq_head - _rxq_tail;
}

static void _rxq_put(const uint8_t *data, uint32_t len)
{
    uint32_t head = _rxq_head;
    for(uint32_t i = 0; i < len; i++)
        _rxq[(head + i) % sizeof(_rxq)] = data[i];
    __dmb();
    _rxq_head = head + len;
}

static bool _rxq_get(uint8_t *channel, uint8_t *data, uint16_t *len, uint16_t max)
{
    if(_rxq_used() < 3) return false;

    uint32_t tail = _rxq_tail;
    *channel = _rxq[tail % sizeof(_rxq)];
    uint16_t n = _rxq[(tail + 1) % sizeof(_rxq)] | (_rxq[(tail + 2) % sizeof(_rxq)] << 8);
    for(uint16_t i = 0; i < n; i++)
    {
        uint8_t b = _rxq[(tail + 3 + i) % sizeof(_rxq)];
        if(i < max) data[i] = b;
    }
    __dmb();
    _rxq_tail = tail + 3 + n;
    *len = n < max ? n : max;
    return true;
}

// Link receive, from the engine interrupt. Refuse if there is no room, the ESP32 repeats it.
static bool _link_rx(void *ctx, uint8_t channel, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if((sizeof(_rxq) - _rxq_used()) < (uint32_t)len + 3)
        return channel == HLINK_CH_IDLE;

    uint8_t head[3] = {channel, len & 0xFF, len >> 8};
    _rxq_put(head, 3);
    if(len) _rxq_put(data, len);
    return true;
}

static void _rx_notify();

static void _engine_start()
{
    if(_busy || _paused) return;

    i2c_hw_t *hw = i2c_get_hw(LINK_I2C);

    if(_reconfigure)
    {
        i2c_set_baudrate(LINK_I2C, ESP32_HCI_I2C_KHZ * 1000);
        // Both frames and addresses at 9 clocks a byte, plus the gap
        uint32_t exchange_us = ((HLINK_FRAME_SIZE + 1) * 2 * 9 * 1000) / ESP32_HCI_I2C_KHZ + ENGINE_READ_GAP_US;
        _timeout_us = exchange_us * ENGINE_TIMEOUT_EXCHANGES;
        if(_timeout_us < ENGINE_TIMEOUT_MIN_US) _timeout_us = ENGINE_TIMEOUT_MIN_US;

        hw->enable = 0;
        hw->tar = ESP32_HCI_I2C_ADDRESS;
        hw->enable = 1;
        _reconfigure = false;
    }

    uint8_t frame[HLINK_FRAME_SIZE];
    uint32_t save = spin_lock_blocking(_lock);
    hlink_build(&_link, frame);
    spin_unlock(_lock, save);

    // Write with a STOP, then the read commands
    for(int i = 0; i < HLINK_FRAME_SIZE; i++)
        _cmd[i] = frame[i];
    _cmd[HLINK_FRAME_SIZE - 1] |= I2C_IC_DATA_CMD_STOP_BITS;
    for(int i = HLINK_FRAME_SIZE; i < HLINK_FRAME_SIZE * 2; i++)
        _cmd[i] = I2C_IC_DATA_CMD_CMD_BITS;
    _cmd[HLINK_FRAME_SIZE * 2 - 1] |= I2C_IC_DATA_CMD_STOP_BITS;

    (void)hw->clr_tx_abrt;
    (void)hw->clr_stop_det;
    hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS;

    _busy = true;
    _phase = 1;
    _busy_since_us = sys_hal_now_us();
    dma_channel_set_write_addr(_dma_rx, _rx_frame, false);
    dma_channel_set_trans_count(_dma_rx, HLINK_FRAME_SIZE, true);
    dma_channel_set_read_addr(_dma_tx, _cmd, false);
    dma_channel_set_trans_count(_dma_tx, HLINK_FRAME_SIZE, true);
}

// End of the write: queue the read after the gap
static void _engine_i2c_irq()
{
    i2c_hw_t *hw = i2c_get_hw(LINK_I2C);
    if(!(hw->intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS)) return;
    (void)hw->clr_stop_det;
    if(_phase != 1) return;

    if(hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS)
    {
        // The ESP32 NACKs the byte that fills its FIFO but keeps it. Only an address NACK is
        // a failure, the task loop handles that one.
        if(hw->tx_abrt_source & I2C_IC_TX_ABRT_SOURCE_ABRT_7B_ADDR_NOACK_BITS) return;
        (void)hw->clr_tx_abrt;
    }

    hw->intr_mask = 0;
    _phase = 2;
    busy_wait_us_32(ENGINE_READ_GAP_US);
    dma_channel_set_read_addr(_dma_tx, &_cmd[HLINK_FRAME_SIZE], false);
    dma_channel_set_trans_count(_dma_tx, HLINK_FRAME_SIZE, true);
}

static void _engine_fail(bool nack)
{
    i2c_hw_t *hw = i2c_get_hw(LINK_I2C);

    dma_channel_abort(_dma_tx);
    dma_channel_abort(_dma_rx);
    dma_channel_acknowledge_irq1(_dma_rx);

    // Drop anything still queued in the controller. A held transfer must be aborted
    // before it can be disabled.
    if(hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS)
    {
        hw->enable |= I2C_IC_ENABLE_ABORT_BITS;
        uint64_t until = sys_hal_now_us() + 2000;
        while((hw->enable & I2C_IC_ENABLE_ABORT_BITS) && sys_hal_now_us() < until)
            tight_loop_contents();
    }
    hw->enable = 0;
    uint64_t until = sys_hal_now_us() + 2000;
    while((hw->enable_status & I2C_IC_ENABLE_STATUS_IC_EN_BITS) && sys_hal_now_us() < until)
        tight_loop_contents();

    (void)hw->clr_tx_abrt;
    (void)hw->clr_stop_det;
    hw->intr_mask = 0;
    _reconfigure = true;
    _phase = 0;

    if(nack) _eng.nacks++;
    else _eng.timeouts++;

    _next_start_us = sys_hal_now_us() + ENGINE_BACKOFF_US;
    _busy = false;
}

static void _engine_irq()
{
    if(!(dma_hw->ints1 & (1u << _dma_rx))) return;
    dma_channel_acknowledge_irq1(_dma_rx);
    if(!_busy) return;

    uint32_t save = spin_lock_blocking(_lock);
    hlink_receive(&_link, _rx_frame);
    spin_unlock(_lock, save);

    _eng.exchanges++;
    _phase = 0;
    _busy = false;
    _rx_notify();

    if(_altflash)
        _next_start_us = _busy_since_us + ENGINE_UPDATE_PERIOD_US;
    else
        _engine_start();
}

// From the task loop: catch failures, start delayed exchanges
static void _engine_poll(uint64_t now_us)
{
    uint32_t irq = save_and_disable_interrupts();

    if(_busy)
    {
        i2c_hw_t *hw = i2c_get_hw(LINK_I2C);
        if((hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) &&
           (hw->tx_abrt_source & I2C_IC_TX_ABRT_SOURCE_ABRT_7B_ADDR_NOACK_BITS))
            _engine_fail(true);
        else if((now_us - _busy_since_us) > _timeout_us)
            _engine_fail(false);
    }
    else if(!_paused && (int64_t)(now_us - _next_start_us) >= 0)
    {
        _engine_start();
    }

    restore_interrupts(irq);
}

// Other drivers on this bus (the charger) get it between exchanges
static void _bus_acquire()
{
    _paused = true;
    while(_busy)
        _engine_poll(sys_hal_now_us());
    i2c_get_hw(LINK_I2C)->intr_mask = 0;
    i2c_set_baudrate(LINK_I2C, i2c_hal_get_baudrate(BLUETOOTH_DRIVER_I2C_INSTANCE));
    _reconfigure = true;
}

static void _bus_release()
{
    _paused = false;
    _next_start_us = sys_hal_now_us();
}

static const i2c_hal_bus_hooks_s _bus_hooks = {
    .acquire = _bus_acquire,
    .release = _bus_release,
};

static void _engine_init()
{
    i2c_hw_t *hw = i2c_get_hw(LINK_I2C);

    _dma_tx = dma_claim_unused_channel(true);
    _dma_rx = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(_dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, i2c_get_dreq(LINK_I2C, true));
    dma_channel_configure(_dma_tx, &c, &hw->data_cmd, _cmd, 0, false);

    c = dma_channel_get_default_config(_dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, i2c_get_dreq(LINK_I2C, false));
    dma_channel_configure(_dma_rx, &c, _rx_frame, &hw->data_cmd, 0, false);

    dma_channel_set_irq1_enabled(_dma_rx, true);
    irq_add_shared_handler(DMA_IRQ_1, _engine_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);

    hw->intr_mask = 0;
    uint i2c_irq = BLUETOOTH_DRIVER_I2C_INSTANCE ? I2C1_IRQ : I2C0_IRQ;
    irq_set_exclusive_handler(i2c_irq, _engine_i2c_irq);
    irq_set_enabled(i2c_irq, true);

    i2c_hal_set_bus_hooks(BLUETOOTH_DRIVER_I2C_INSTANCE, &_bus_hooks);
    _reconfigure = true;
    _next_start_us = sys_hal_now_us();
}

void esp32_hci_log(const char *fmt, ...)
{
    char line[160];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if(n <= 0) return;
    if(n >= (int)sizeof(line)) n = sizeof(line) - 1;
    _link_send(HLINK_CH_LOG, (const uint8_t *)line, n);
}

#if ESP32_HCI_CONSOLE
#include "pico/stdio/driver.h"

// printf goes to the bridge console a line at a time
static char _stdio_line[160];
static uint32_t _stdio_len = 0;

static void _stdio_out(const char *buf, int len)
{
    uint32_t irq = save_and_disable_interrupts();
    for(int i = 0; i < len; i++)
    {
        char c = buf[i];
        if(c == '\r') continue;
        if(c != '\n') _stdio_line[_stdio_len++] = c;

        if(c == '\n' || _stdio_len == sizeof(_stdio_line))
        {
            if(_stdio_len) _link_send(HLINK_CH_LOG, (const uint8_t *)_stdio_line, _stdio_len);
            _stdio_len = 0;
        }
    }
    restore_interrupts(irq);
}

static stdio_driver_t _stdio_driver = {
    .out_chars = _stdio_out,
    .crlf_enabled = false,
};

static void _console(const char *line)
{
    if(!strcmp(line, "bootsel"))
    {
        sys_hal_bootloader();
    }
    else if(!strcmp(line, "reboot"))
    {
        sys_hal_reboot();
    }
    else if(!strcmp(line, "bbupdate"))
    {
        boot_memory_s mem = {.baseband_update = true};
        boot_set_memory(&mem);
        sys_hal_reboot();
    }
    else if(!strncmp(line, "mode ", 5) || !strcmp(line, "pair"))
    {
        boot_memory_s mem = {0};
        if(!strcmp(line, "pair"))
        {
            mem.report_format = _params->core_report_format;
            mem.gamepad_pair = 1;
        }
        else if(!strcmp(&line[5], "switch")) mem.report_format = CORE_REPORTFORMAT_SWPRO;
        else if(!strcmp(&line[5], "sinput")) mem.report_format = CORE_REPORTFORMAT_SINPUT;
        else if(!strcmp(&line[5], "wii"))    mem.report_format = CORE_REPORTFORMAT_WII;
        else
        {
            esp32_hci_log("modes: switch sinput wii");
            return;
        }
        mem.gamepad_method = GAMEPAD_METHOD_BLUETOOTH;
        boot_set_memory(&mem);
        sys_hal_reboot();
    }
    else if(!strcmp(line, "bt"))
    {
        static const char *states[] = {"off", "initializing", "working", "halting", "sleeping", "falling asleep"};
        bd_addr_t addr;
        gap_local_bd_addr(addr);
        HCI_STATE st = hci_get_state();
        esp32_hci_log("hci %s, address %s, mode %u, esp32 fw 0x%04X, battery %u mV",
                      st < 6 ? states[st] : "?", bd_addr_to_str(addr), _params->core_report_format,
                      _esp32_version, _battery_mv);
    }
    else if(!strcmp(line, "stats"))
    {
        hlink_stats_s s;
        uint32_t save = spin_lock_blocking(_lock);
        s = _link.stats;
        spin_unlock(_lock, save);
        esp32_hci_log("link tx %lu rx %lu | crc %lu ooo %lu retx %lu refused %lu drop %lu restarts %lu",
                      s.frames_tx, s.frames_rx, s.crc_errors, s.out_of_order, s.retransmits, s.refused,
                      s.packets_dropped, s.peer_restarts);
        esp32_hci_log("engine %lu exchanges, %lu nack, %lu timeout",
                      _eng.exchanges, _eng.nacks, _eng.timeouts);
    }
    else
    {
        esp32_hci_log("commands: bootsel reboot bbupdate | mode switch/sinput/wii | pair | bt | stats");
    }
}
#endif

static void _handle_ctrl(const uint8_t *msg, uint16_t len)
{
    if(!len) return;

    switch(msg[0])
    {
        case HLINK_CTRL_HELLO_RSP:
        if(len >= 4)
            esp32_hci_log("bridge fw 0x%02X%02X, link up%s", msg[2], msg[3], _altflash ? " (update mode)" : "");
        break;

        case HLINK_CTRL_RADIO_READY:
        if(len >= 2 && msg[1])
            esp32_hci_log("radio failed to start (%u)", msg[1]);
        break;

        case HLINK_CTRL_BATTERY:
        if(len >= 3)
            _battery_mv = (msg[1] << 8) | msg[2];
        break;

        #if ESP32_HCI_CONSOLE
        case HLINK_CTRL_CONSOLE:
        {
            char line[64];
            uint16_t n = len - 1;
            if(n >= sizeof(line)) n = sizeof(line) - 1;
            memcpy(line, &msg[1], n);
            line[n] = 0;
            _console(line);
        }
        break;
        #endif

        default:
        break;
    }
}

// HCI transport for BTstack. HCI packets use the link channels matching their H4 type.
// While open, BTstack's run loop reads the receive queue; otherwise the task loop does.
static void (*_hci_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;
static btstack_data_source_t _hci_source;
static volatile bool _hci_open = false;
static bool _radio_requested = false;

// BTstack may use the space in front of a packet
static uint8_t _hci_in[HCI_INCOMING_PRE_BUFFER_SIZE + HLINK_PACKET_MAX];

static void _request_radio()
{
    // The bridge starts its controller with our address. HCI packets queued after this
    // reach it once it is up.
    uint8_t msg[7] = {HLINK_CTRL_START_RADIO};
    memcpy(&msg[1], _params->transport_dev_mac, 6);
    _link_send(HLINK_CH_CTRL, msg, sizeof(msg));
    _radio_requested = true;

    #if defined(BLUETOOTH_DRIVER_BATMON_ENABLE) && (BLUETOOTH_DRIVER_BATMON_ENABLE == 1)
    uint8_t batmon[3] = {HLINK_CTRL_BATMON, 0, 0};
    if(battery_pack_state() != BATTERY_PACK_ABSENT)
    {
        batmon[1] = 1;
        batmon[2] = BLUETOOTH_DRIVER_BATMON_ADC_GPIO;
    }
    _link_send(HLINK_CH_CTRL, batmon, sizeof(batmon));
    #endif
}

static void _handle_packet(uint8_t channel, uint8_t *data, uint16_t len)
{
    switch(channel)
    {
        // The bridge restarted
        case HLINK_CH_IDLE:
        {
            uint8_t hello[] = {HLINK_CTRL_HELLO, HLINK_PROTOCOL_VERSION};
            _link_send(HLINK_CH_CTRL, hello, sizeof(hello));
            if(_radio_requested) _request_radio();
        }
        break;

        case HLINK_CH_HCI_EVT:
        case HLINK_CH_HCI_ACL:
        case HLINK_CH_HCI_SCO:
        if(_hci_open && _hci_handler)
            _hci_handler(channel, data, len);
        break;

        case HLINK_CH_CTRL:
        _handle_ctrl(data, len);
        break;

        default:
        break;
    }
}

static void _process_rx()
{
    uint8_t *data = &_hci_in[HCI_INCOMING_PRE_BUFFER_SIZE];
    uint8_t channel;
    uint16_t len;
    while(_rxq_get(&channel, data, &len, HLINK_PACKET_MAX))
        _handle_packet(channel, data, len);
}

// From the engine interrupt
static void _rx_notify()
{
    if(_hci_open && _rxq_used())
        btstack_run_loop_poll_data_sources_from_irq();
}

static void _hci_source_poll(btstack_data_source_t *ds, btstack_data_source_callback_type_t type)
{
    (void)ds;
    (void)type;
    _process_rx();
}

static void _hci_init(const void *config)
{
    (void)config;
}

// Keeps the run loop servicing the link. Without it, SInput reports stall for up to a second
// waiting for something else to wake BTstack.
#define HCI_TICK_MS 8
static btstack_timer_source_t _tick_timer;
static void _tick_timer_handler(btstack_timer_source_t *ts)
{
    btstack_run_loop_set_timer(ts, HCI_TICK_MS);
    btstack_run_loop_add_timer(ts);
}

static int _hci_open_fn()
{
    btstack_run_loop_set_timer_handler(&_tick_timer, _tick_timer_handler);
    btstack_run_loop_set_timer(&_tick_timer, HCI_TICK_MS);
    btstack_run_loop_add_timer(&_tick_timer);

    if(!_radio_requested) _request_radio();

    btstack_run_loop_set_data_source_handler(&_hci_source, &_hci_source_poll);
    btstack_run_loop_enable_data_source_callbacks(&_hci_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&_hci_source);
    _hci_open = true;
    return 0;
}

static int _hci_close_fn()
{
    _hci_open = false;
    btstack_run_loop_remove_timer(&_tick_timer);
    btstack_run_loop_disable_data_source_callbacks(&_hci_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&_hci_source);
    return 0;
}

static void _hci_register_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size))
{
    _hci_handler = handler;
}

static int _hci_can_send_now(uint8_t packet_type)
{
    (void)packet_type;
    return _link_tx_free() >= hlink_packet_cost(HLINK_PACKET_MAX);
}

static int _hci_send_packet(uint8_t packet_type, uint8_t *packet, int size)
{
    // BTstack only sends what the controller has room for, so wait for room rather than fail
    uint64_t until = sys_hal_now_us() + 100000;
    while(!_link_send(packet_type, packet, size))
    {
        _engine_poll(sys_hal_now_us());
        if(sys_hal_now_us() > until)
        {
            esp32_hci_log("hci: link full, packet dropped");
            break;
        }
    }

    static uint8_t sent[] = {HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    if(_hci_handler) _hci_handler(HCI_EVENT_PACKET, sent, sizeof(sent));
    return 0;
}

static const hci_transport_t _hci_transport = {
    .name = "HOJA-ESP32",
    .init = _hci_init,
    .open = _hci_open_fn,
    .close = _hci_close_fn,
    .register_packet_handler = _hci_register_handler,
    .can_send_packet_now = _hci_can_send_now,
    .send_packet = _hci_send_packet,
    .set_baudrate = NULL,
    .reset_link = NULL,
    .set_sco_config = NULL,
};

const void *esp32_hci_transport_instance(void)
{
    return &_hci_transport;
}

void esp32_hci_backend_stop(void)
{
    _esp32_enable(false);
}

void esp32_hci_console_attach(void)
{
    #if ESP32_HCI_CONSOLE
    _esp32_usb_select(true);
    #endif
}

bool esp32_hci_backend_update_mode(void)
{
    return _altflash;
}

bool esp32_hci_backend_init(core_params_s *params)
{
    _params = params;
    if(!_params) return false;

    HOJA_USB_MUX_INIT();

    _lock = spin_lock_init(spin_lock_claim_unused(true));
    hlink_init(&_link, _txq, sizeof(_txq), sys_hal_random(), _link_rx, NULL);

    _altflash = (params->core_boot_flags & COREBOOT_FLAG_ALTFLASH) != 0;

    // Fresh ESP32 boot so the controller gets this run's address
    _esp32_enable(false);
    sys_hal_sleep_ms(10);

    if(_altflash || ESP32_HCI_CONSOLE)
        _esp32_usb_select(true);

    _esp32_enable(true);

    uint8_t hello[] = {HLINK_CTRL_HELLO, HLINK_PROTOCOL_VERSION};
    _link_send(HLINK_CH_CTRL, hello, sizeof(hello));

    _engine_init();

    #if ESP32_HCI_CONSOLE
    stdio_set_driver_enabled(&_stdio_driver, true);
    #endif

    return true;
}

void esp32_hci_backend_task(uint64_t timestamp)
{
    if(!_params) return;

    _engine_poll(timestamp);
    if(!_hci_open) _process_rx();
}

// The old baseband and the bridge both answer the old version request
static uint16_t _esp32_read_version()
{
    uint8_t out[LEGACY_MSG_SIZE_OUT] = {LEGACY_CMD_FW_VERSION};
    uint8_t in[LEGACY_MSG_SIZE_IN];

    _esp32_enable(false);
    sys_hal_sleep_ms(10);
    _esp32_enable(true);
    sys_hal_sleep_ms(600);

    for(int attempt = 0; attempt < 10; attempt++)
    {
        i2c_hal_write_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, ESP32_HCI_I2C_ADDRESS, out, sizeof(out), false, 10000);
        sys_hal_sleep_ms(4);
        int r = i2c_hal_read_timeout_us(BLUETOOTH_DRIVER_I2C_INSTANCE, ESP32_HCI_I2C_ADDRESS, in, sizeof(in), false, 10000);

        if(r == sizeof(in))
        {
            uint16_t version = (in[1] << 8) | in[2];
            if(version)
            {
                _esp32_enable(false);
                return version;
            }
        }
        sys_hal_sleep_ms(20);
    }

    _esp32_enable(false);
    return 0;
}

uint16_t esp32_hci_firmware_version(void)
{
    if(!_esp32_version_read)
    {
        _esp32_version = _esp32_read_version();
        _esp32_version_read = true;
    }
    return _esp32_version;
}

uint16_t esp32_hci_battery_mv(void)
{
    return _battery_mv;
}

void transport_bt_static_get_caps(transport_bt_static_caps_s *caps)
{
    if(!caps) return;

    caps->bdr_supported = 1;
    caps->ble_supported = 0;
    caps->external_update_supported = 1;
}

uint8_t transport_bt_static_part_status(void)
{
    _esp32_version = _esp32_read_version();
    _esp32_version_read = true;
    return _esp32_version ? TRANSPORT_WIRELESS_PART_OK : TRANSPORT_WIRELESS_PART_ERROR;
}

uint16_t transport_bt_static_external_version(void)
{
    return _esp32_version;
}

const char *bluetooth_driver_part_code(void)
{
    return "ESP32 HCI";
}

// Battery level from the bridge's voltage reading (R4K)
#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ESP32)

// Single-cell Li-ion, mV to percent
static const uint16_t _fg_curve[][2] = {
    {4150, 100}, {4050, 90}, {3970, 80}, {3900, 70}, {3840, 60}, {3790, 50},
    {3750, 40}, {3710, 30}, {3670, 20}, {3600, 10}, {3450, 5}, {3300, 0},
};

static uint8_t _fg_percent(uint16_t mv)
{
    const int n = sizeof(_fg_curve) / sizeof(_fg_curve[0]);
    if(mv >= _fg_curve[0][0]) return 100;

    for(int i = 1; i < n; i++)
    {
        if(mv >= _fg_curve[i][0])
        {
            uint16_t v0 = _fg_curve[i][0], v1 = _fg_curve[i - 1][0];
            uint16_t p0 = _fg_curve[i][1], p1 = _fg_curve[i - 1][1];
            return p0 + ((uint32_t)(mv - v0) * (p1 - p0)) / (v1 - v0);
        }
    }
    return 0;
}

bool fuelgauge_driver_init(uint16_t capacity_mah)
{
    (void)capacity_mah;
    return true;
}

fuelgauge_status_s fuelgauge_driver_get_status(void)
{
    fuelgauge_status_s status = {0};
    uint16_t mv = _battery_mv;
    status.connected = mv != 0;
    status.percent = mv ? _fg_percent(mv) : 100;
    status.discharge_only = true; // Charging raises the voltage
    return status;
}

const char *fuelgauge_driver_part_code(void)
{
    return "ESP32";
}

#endif

#endif
