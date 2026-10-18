#include "boot_usb.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

#include "tusb.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"

#include "hal/rp2040/boot_layout.h"
#include "boot_flash.h"

// Set by the build for each board
#ifndef BOOT_DEVICE_NAME
#define BOOT_DEVICE_NAME "HOJA Gamepad"
#endif
#ifndef BOOT_BUILD_ID
#define BOOT_BUILD_ID "hoja"
#endif

#define BOOT_USB_RHPORT     0
#define BOOT_USB_EP_OUT     0x01u
#define BOOT_USB_EP_IN      0x81u
#define BOOT_USB_PACKET_LEN 64u
#define BOOT_USB_VENDOR_MS  0x01u // The MS OS 2.0 vendor request, named in the BOS descriptor
#define BOOT_USB_MS_OS_20_INDEX 7u
#define BOOT_USB_POWER_MA   100u

// The update interface's own WinUSB device interface GUID (tools find the update mode by it)
#define BOOT_USB_INTERFACE_GUID "{2C8F6F2C-8CEF-4D98-8035-CE6AC66AB70A}"

// With nothing written, the update mode gives way to the firmware once the host is gone: never
// enumerated (the cable came out before the reboot), or suspended (unplugged, or the host asleep)
#define BOOT_USB_UNUSED_MS    10000u
#define BOOT_USB_SUSPENDED_MS 3000u
// A fault in the update mode restarts the chip rather than hang it (on battery nothing else would);
// the longest wait between loop passes is one sector's erase
#define BOOT_USB_WATCHDOG_MS 2000u
// The acknowledgement leaves before the chip restarts
#define BOOT_USB_REBOOT_MIN_MS 10u
#define BOOT_USB_REBOOT_MAX_MS 2000u

// --- PICOBOOT (the RP2040 boot ROM's protocol, as the config app and picotool speak it) ---

#define BOOT_PB_MAGIC            0x431FD10Bu
#define BOOT_PB_CMD_EXCLUSIVE    0x01u
#define BOOT_PB_CMD_REBOOT       0x02u
#define BOOT_PB_CMD_FLASH_ERASE  0x03u
#define BOOT_PB_CMD_WRITE        0x05u
#define BOOT_PB_CMD_EXIT_XIP     0x06u
#define BOOT_PB_CMD_ENTER_XIP    0x07u
#define BOOT_PB_CMD_READ         0x84u
#define BOOT_PB_CMD_DIR_IN       0x80u
#define BOOT_PB_IF_RESET         0x41u
#define BOOT_PB_IF_CMD_STATUS    0x42u
#define BOOT_PB_CMD_LEN          32u
#define BOOT_PB_STATUS_LEN       16u
#define BOOT_FLASH_PAGE_LEN      256u

typedef enum
{
    BOOT_PB_OK = 0,
    BOOT_PB_UNKNOWN_CMD = 1,
    BOOT_PB_INVALID_CMD_LENGTH = 2,
    BOOT_PB_INVALID_TRANSFER_LENGTH = 3,
    BOOT_PB_INVALID_ADDRESS = 4,
    BOOT_PB_BAD_ALIGNMENT = 5,
    BOOT_PB_REBOOTING = 7,
} boot_pb_status_t;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t token;
    uint8_t  cmd_id;
    uint8_t  cmd_size;
    uint16_t _unused;
    uint32_t transfer_length;
    uint32_t args[4]; // Range: address, size. Reboot: pc, sp, delay in ms.
} boot_pb_cmd_s;

typedef enum
{
    BOOT_PB_STATE_IDLE,     // Waiting for a command
    BOOT_PB_STATE_DATA_OUT, // Receiving a write's data
    BOOT_PB_STATE_DATA_IN,  // Sending a read's data, then the host's empty acknowledgement
    BOOT_PB_STATE_ACK,      // Sending our empty acknowledgement
} boot_pb_state_t;

static const tusb_desc_device_t _boot_usb_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0210, // 2.1: the host asks for the BOS, and with it the MS OS 2.0 set
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = BOOT_USB_VID,
    .idProduct          = BOOT_USB_PID,
    .bcdDevice          = BOOT_USB_BCD,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1,
};

#define BOOT_USB_CONFIG_LEN (TUD_CONFIG_DESC_LEN + 9u + 7u + 7u)

static const uint8_t _boot_usb_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, BOOT_USB_CONFIG_LEN, 0x00, BOOT_USB_POWER_MA),
    // The vendor interface as the boot ROM's PICOBOOT one; its name is the build id
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 4,
    7, TUSB_DESC_ENDPOINT, BOOT_USB_EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(BOOT_USB_PACKET_LEN), 0,
    7, TUSB_DESC_ENDPOINT, BOOT_USB_EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(BOOT_USB_PACKET_LEN), 0,
};

// The MS OS 2.0 set for a single-interface device: WinUSB, and the interface GUID
#define BOOT_USB_MS_HEADER_LEN   0x0Au
#define BOOT_USB_MS_COMPAT_LEN   0x14u
#define BOOT_USB_MS_PROP_NAME    "DeviceInterfaceGUIDs"
#define BOOT_USB_MS_PROP_NAME_LEN ((sizeof(BOOT_USB_MS_PROP_NAME)) * 2u)         // UTF-16, with its NUL
#define BOOT_USB_MS_PROP_DATA_LEN ((sizeof(BOOT_USB_INTERFACE_GUID) + 1u) * 2u) // A multi-string: two NULs
#define BOOT_USB_MS_PROP_LEN     (10u + BOOT_USB_MS_PROP_NAME_LEN + BOOT_USB_MS_PROP_DATA_LEN)
#define BOOT_USB_MS_OS_20_LEN    (BOOT_USB_MS_HEADER_LEN + BOOT_USB_MS_COMPAT_LEN + BOOT_USB_MS_PROP_LEN)

#define BOOT_USB_BOS_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static const uint8_t _boot_usb_bos[] = {
    TUD_BOS_DESCRIPTOR(BOOT_USB_BOS_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(BOOT_USB_MS_OS_20_LEN, BOOT_USB_VENDOR_MS),
};

static uint8_t _boot_usb_ms_os_20[BOOT_USB_MS_OS_20_LEN];
static char _boot_usb_serial[2u * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1u];
static uint16_t _boot_usb_string[48];

static uint8_t _boot_pb_rhport;
static boot_pb_state_t _boot_pb_state;
static boot_pb_cmd_s _boot_pb_cmd;
static uint32_t _boot_pb_token;
static boot_pb_status_t _boot_pb_status;
static uint8_t __attribute__((aligned(4))) _boot_pb_packet[BOOT_USB_PACKET_LEN];
static uint8_t __attribute__((aligned(4))) _boot_pb_data[FLASH_SECTOR_SIZE];

// The application slot's first sector, as the host has written it: committed to flash only at the
// restart, so a transfer cut short leaves no vector table and the bootloader stays in update mode
static uint8_t _boot_first_sector[FLASH_SECTOR_SIZE];
static bool _boot_first_sector_written;

static bool _boot_dirty;       // Flash changed: the firmware may be partial, so stay
static bool _boot_mounted;
static bool _boot_suspended;
static uint32_t _boot_suspended_at_ms;
static uint32_t _boot_restart_at_ms;
static bool _boot_restart_pending;
static bool _boot_restart_rom;

// --- Restart ---

static void _boot_commit_first_sector(void)
{
    if (!_boot_first_sector_written)
        return;

    boot_flash_write_sector(BOOT_LAYOUT_APP_OFFSET, _boot_first_sector);
    _boot_first_sector_written = false;
}

static void __attribute__((noreturn)) _boot_restart(bool rom)
{
    _boot_commit_first_sector();
    tud_disconnect();

    if (rom)
        reset_usb_boot(0, 0);

    watchdog_reboot(0, 0, 0);
    for (;;)
        tight_loop_contents();
}

static void _boot_schedule_restart(uint32_t delay_ms, bool rom)
{
    delay_ms = MAX(delay_ms, BOOT_USB_REBOOT_MIN_MS);
    delay_ms = MIN(delay_ms, BOOT_USB_REBOOT_MAX_MS);
    _boot_restart_at_ms = to_ms_since_boot(get_absolute_time()) + delay_ms;
    _boot_restart_rom = rom;
    _boot_restart_pending = true;
}

// --- Flash access through the PICOBOOT commands ---

// The bootloader never rewrites itself (it runs from there): writes to it are acknowledged and
// dropped, so an image that carries it can be written whole
static bool _boot_in_bootloader(uint32_t offset)
{
    return offset < BOOT_LAYOUT_BOOT_SIZE;
}

static bool _boot_in_first_sector(uint32_t offset)
{
    return (offset >= BOOT_LAYOUT_APP_OFFSET) && (offset < BOOT_LAYOUT_APP_OFFSET + FLASH_SECTOR_SIZE);
}

static bool _boot_range_valid(uint32_t addr, uint32_t len, uint32_t *offset)
{
    if ((addr < XIP_BASE) || (len > PICO_FLASH_SIZE_BYTES) ||
        (addr - XIP_BASE > PICO_FLASH_SIZE_BYTES - len))
        return false;

    *offset = addr - XIP_BASE;
    return true;
}

static boot_pb_status_t _boot_erase(uint32_t addr, uint32_t len)
{
    uint32_t offset;

    if (!_boot_range_valid(addr, len, &offset))
        return BOOT_PB_INVALID_ADDRESS;

    if ((offset % FLASH_SECTOR_SIZE) || (len % FLASH_SECTOR_SIZE))
        return BOOT_PB_BAD_ALIGNMENT;

    for (uint32_t at = offset; at < offset + len; at += FLASH_SECTOR_SIZE)
    {
        if (_boot_in_bootloader(at))
            continue;

        if (_boot_in_first_sector(at))
        {
            memset(_boot_first_sector, 0xFF, sizeof(_boot_first_sector));
            _boot_first_sector_written = false;
        }

        boot_flash_erase(at, FLASH_SECTOR_SIZE);
        _boot_dirty = true;
    }
    return BOOT_PB_OK;
}

static boot_pb_status_t _boot_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    for (uint32_t done = 0; done < len; done += BOOT_FLASH_PAGE_LEN)
    {
        uint32_t at = offset + done;

        if (_boot_in_bootloader(at))
            continue;

        if (_boot_in_first_sector(at))
        {
            // Programming only clears bits, as it would on the flash itself
            uint8_t *page = &_boot_first_sector[at - BOOT_LAYOUT_APP_OFFSET];
            for (uint32_t i = 0; i < BOOT_FLASH_PAGE_LEN; i++)
                page[i] &= data[done + i];
            _boot_first_sector_written = true;
        }
        else
        {
            boot_flash_program(at, &data[done], BOOT_FLASH_PAGE_LEN);
        }
        _boot_dirty = true;
    }
    return BOOT_PB_OK;
}

static void _boot_read(uint32_t offset, uint8_t *out, uint32_t len)
{
    memcpy(out, (const uint8_t *)(XIP_BASE + offset), len);

    // The first sector reads back as written, though not yet committed
    for (uint32_t i = 0; i < len; i++)
    {
        if (_boot_in_first_sector(offset + i))
            out[i] = _boot_first_sector[offset + i - BOOT_LAYOUT_APP_OFFSET];
    }
}

// --- PICOBOOT transfers ---

static void _boot_pb_arm_command(void)
{
    _boot_pb_state = BOOT_PB_STATE_IDLE;
    if (!usbd_edpt_busy(_boot_pb_rhport, BOOT_USB_EP_OUT))
        usbd_edpt_xfer(_boot_pb_rhport, BOOT_USB_EP_OUT, _boot_pb_packet, sizeof(_boot_pb_packet));
}

// Start an endpoint again from DATA0 with nothing armed, as a cleared halt requires. TinyUSB's own
// clear drops only the stall bit: a buffer still armed in the controller would panic the next arm.
static void _boot_pb_endpoint_restart(uint8_t ep_addr)
{
    dcd_edpt_stall(_boot_pb_rhport, ep_addr); // Buffer control to the stall alone: the armed buffer goes
    usbd_edpt_clear_stall(_boot_pb_rhport, ep_addr);
}

static void _boot_pb_ack(void)
{
    _boot_pb_state = BOOT_PB_STATE_ACK;
    usbd_edpt_xfer(_boot_pb_rhport, BOOT_USB_EP_IN, NULL, 0);
}

// The host reads the reason with the command status request, and clears it with the reset request
static void _boot_pb_fail(boot_pb_status_t status)
{
    _boot_pb_status = status;
    _boot_pb_state = BOOT_PB_STATE_IDLE;
    usbd_edpt_stall(_boot_pb_rhport, BOOT_USB_EP_OUT);
    usbd_edpt_stall(_boot_pb_rhport, BOOT_USB_EP_IN);
}

static void _boot_pb_finish(boot_pb_status_t status)
{
    if (status != BOOT_PB_OK)
    {
        _boot_pb_fail(status);
        return;
    }

    _boot_pb_status = BOOT_PB_OK;
    _boot_pb_ack();
}

static void _boot_pb_start_data_out(void)
{
    uint32_t len = _boot_pb_cmd.transfer_length;
    uint32_t offset;

    if ((len == 0) || (len > sizeof(_boot_pb_data)) || (len != _boot_pb_cmd.args[1]))
    {
        _boot_pb_fail(BOOT_PB_INVALID_TRANSFER_LENGTH);
        return;
    }

    if (!_boot_range_valid(_boot_pb_cmd.args[0], len, &offset))
    {
        _boot_pb_fail(BOOT_PB_INVALID_ADDRESS);
        return;
    }

    if ((offset % BOOT_FLASH_PAGE_LEN) || (len % BOOT_FLASH_PAGE_LEN))
    {
        _boot_pb_fail(BOOT_PB_BAD_ALIGNMENT);
        return;
    }

    _boot_pb_state = BOOT_PB_STATE_DATA_OUT;
    usbd_edpt_xfer(_boot_pb_rhport, BOOT_USB_EP_OUT, _boot_pb_data, (uint16_t)len);
}

static void _boot_pb_start_data_in(void)
{
    uint32_t len = _boot_pb_cmd.transfer_length;
    uint32_t offset;

    if ((len == 0) || (len > sizeof(_boot_pb_data)) || (len != _boot_pb_cmd.args[1]))
    {
        _boot_pb_fail(BOOT_PB_INVALID_TRANSFER_LENGTH);
        return;
    }

    if (!_boot_range_valid(_boot_pb_cmd.args[0], len, &offset))
    {
        _boot_pb_fail(BOOT_PB_INVALID_ADDRESS);
        return;
    }

    _boot_read(offset, _boot_pb_data, len);
    _boot_pb_state = BOOT_PB_STATE_DATA_IN;
    usbd_edpt_xfer(_boot_pb_rhport, BOOT_USB_EP_IN, _boot_pb_data, (uint16_t)len);
}

static void _boot_pb_command(uint32_t len)
{
    memcpy(&_boot_pb_cmd, _boot_pb_packet, sizeof(_boot_pb_cmd));

    if ((len != BOOT_PB_CMD_LEN) || (_boot_pb_cmd.magic != BOOT_PB_MAGIC))
    {
        _boot_pb_fail(BOOT_PB_INVALID_CMD_LENGTH);
        return;
    }

    _boot_pb_token = _boot_pb_cmd.token;

    // A command without data carries no transfer length
    bool has_data = (_boot_pb_cmd.cmd_id == BOOT_PB_CMD_WRITE) || (_boot_pb_cmd.cmd_id == BOOT_PB_CMD_READ);
    if (!has_data && _boot_pb_cmd.transfer_length)
    {
        _boot_pb_fail(BOOT_PB_INVALID_TRANSFER_LENGTH);
        return;
    }

    switch (_boot_pb_cmd.cmd_id)
    {
        case BOOT_PB_CMD_EXCLUSIVE:
        case BOOT_PB_CMD_EXIT_XIP:
        case BOOT_PB_CMD_ENTER_XIP:
            // Nothing else uses the flash here: always exclusive, always ready
            _boot_pb_finish(BOOT_PB_OK);
            break;

        case BOOT_PB_CMD_REBOOT:
            // To the firmware, whatever entry point is asked for: the bootloader picks what to start
            _boot_schedule_restart(_boot_pb_cmd.args[2], false);
            _boot_pb_finish(BOOT_PB_OK);
            break;

        case BOOT_CMD_ROM_BOOTLOADER:
            _boot_schedule_restart(0, true);
            _boot_pb_finish(BOOT_PB_OK);
            break;

        case BOOT_PB_CMD_FLASH_ERASE:
            _boot_pb_finish(_boot_erase(_boot_pb_cmd.args[0], _boot_pb_cmd.args[1]));
            break;

        case BOOT_PB_CMD_WRITE:
            _boot_pb_start_data_out();
            break;

        case BOOT_PB_CMD_READ:
            _boot_pb_start_data_in();
            break;

        default:
            _boot_pb_fail(BOOT_PB_UNKNOWN_CMD);
            break;
    }
}

static void _boot_pb_out_done(uint32_t len)
{
    switch (_boot_pb_state)
    {
        case BOOT_PB_STATE_IDLE:
            // An empty packet is the host's acknowledgement of a read
            if (len == 0)
                _boot_pb_arm_command();
            else
                _boot_pb_command(len);
            break;

        case BOOT_PB_STATE_DATA_OUT:
            if (len != _boot_pb_cmd.transfer_length)
            {
                _boot_pb_fail(BOOT_PB_INVALID_TRANSFER_LENGTH);
                break;
            }
            _boot_pb_finish(_boot_write(_boot_pb_cmd.args[0] - XIP_BASE, _boot_pb_data, len));
            break;

        default:
            break;
    }
}

static void _boot_pb_in_done(void)
{
    // A read's data is out (the host's empty acknowledgement follows), or our acknowledgement is
    _boot_pb_arm_command();
}

// --- Class driver ---

static void _boot_pb_init(void)
{
    _boot_pb_state = BOOT_PB_STATE_IDLE;
}

static void _boot_pb_reset(uint8_t rhport)
{
    (void)rhport;
    _boot_pb_state = BOOT_PB_STATE_IDLE;
}

static uint16_t _boot_pb_open(uint8_t rhport, tusb_desc_interface_t const *desc, uint16_t max_len)
{
    uint16_t len = (uint16_t)(sizeof(tusb_desc_interface_t) + 2u * sizeof(tusb_desc_endpoint_t));

    if ((desc->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC) || (max_len < len))
        return 0;

    uint8_t ep_out;
    uint8_t ep_in;
    const uint8_t *ep_desc = tu_desc_next(desc);
    if (!usbd_open_edpt_pair(rhport, ep_desc, 2, TUSB_XFER_BULK, &ep_out, &ep_in))
        return 0;

    _boot_pb_rhport = rhport;
    _boot_pb_arm_command();
    return len;
}

// Standard requests to its endpoints come here too (TinyUSB forwards them). Clearing a halt, as
// Windows does when it resets a pipe, leaves the endpoint idle: wait for a command again.
static bool _boot_pb_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    (void)rhport;

    if ((stage != CONTROL_STAGE_SETUP) || (request->bRequest != TUSB_REQ_CLEAR_FEATURE))
        return true;

    // Whatever was under way on either endpoint is over: wait for a command
    _boot_pb_endpoint_restart(tu_u16_low(request->wIndex));
    _boot_pb_arm_command();
    return true;
}

static bool _boot_pb_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t len)
{
    (void)rhport;

    if (result != XFER_RESULT_SUCCESS)
        return true;

    if (ep_addr == BOOT_USB_EP_OUT)
        _boot_pb_out_done(len);
    else if (ep_addr == BOOT_USB_EP_IN)
        _boot_pb_in_done();

    return true;
}

static const usbd_class_driver_t _boot_pb_driver = {
    .name            = "picoboot",
    .init            = _boot_pb_init,
    .reset           = _boot_pb_reset,
    .open            = _boot_pb_open,
    .control_xfer_cb = _boot_pb_control_xfer_cb, // PICOBOOT's own are vendor: tud_vendor_control_xfer_cb
    .xfer_cb         = _boot_pb_xfer_cb,
    .sof             = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &_boot_pb_driver;
}

// --- Control requests ---

static bool _boot_pb_interface_request(uint8_t rhport, tusb_control_request_t const *request)
{
    static uint8_t status[BOOT_PB_STATUS_LEN];

    switch (request->bRequest)
    {
        case BOOT_PB_IF_RESET:
            _boot_pb_endpoint_restart(BOOT_USB_EP_OUT);
            _boot_pb_endpoint_restart(BOOT_USB_EP_IN);
            _boot_pb_status = BOOT_PB_OK;
            _boot_pb_arm_command();
            return tud_control_status(rhport, request);

        case BOOT_REQUEST_LAYOUT:
        {
            static const boot_layout_report_s layout = {
                .version             = BOOT_STAGE_VERSION,
                .flash_size          = PICO_FLASH_SIZE_BYTES,
                .radio_offset        = BOOT_LAYOUT_RADIO_OFFSET,
                .radio_size          = BOOT_LAYOUT_RADIO_SIZE,
                .app_offset          = BOOT_LAYOUT_APP_OFFSET,
                .stage_header_offset = BOOT_LAYOUT_STAGE_HEADER_OFFSET,
                .top_offset          = BOOT_LAYOUT_TOP_OFFSET,
            };
            return tud_control_xfer(rhport, request, (void *)&layout, sizeof(layout));
        }

        case BOOT_REQUEST_FAULT:
            memcpy(status, (const void *)&watchdog_hw->scratch[BOOT_FAULT_SCRATCH_IDX], 8);
            return tud_control_xfer(rhport, request, status, 8);

        case BOOT_PB_IF_CMD_STATUS:
            memset(status, 0, sizeof(status));
            memcpy(&status[0], &_boot_pb_token, 4);
            status[4] = (uint8_t)_boot_pb_status;
            status[8] = _boot_pb_cmd.cmd_id;
            return tud_control_xfer(rhport, request, status, sizeof(status));

        default:
            return false;
    }
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    if (stage != CONTROL_STAGE_SETUP)
        return true;

    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE)
        return _boot_pb_interface_request(rhport, request);

    if ((request->bRequest == BOOT_USB_VENDOR_MS) && (request->wIndex == BOOT_USB_MS_OS_20_INDEX))
        return tud_control_xfer(rhport, request, _boot_usb_ms_os_20, sizeof(_boot_usb_ms_os_20));

    return false;
}

// --- Descriptors ---

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&_boot_usb_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return _boot_usb_config;
}

uint8_t const *tud_descriptor_bos_cb(void)
{
    return _boot_usb_bos;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    const char *text = NULL;
    uint8_t count = 0;

    switch (index)
    {
        case 0:
            _boot_usb_string[1] = 0x0409; // English (US)
            count = 1;
            break;

        case 1:
            text = "Hand Held Legend";
            break;

        case 2:
            text = BOOT_DEVICE_NAME " Update";
            break;

        case 3:
            text = _boot_usb_serial;
            break;

        case 4:
            text = BOOT_BUILD_ID;
            break;

        default:
            return NULL;
    }

    if (index != 0)
    {
        for (; text[count] && (count < TU_ARRAY_SIZE(_boot_usb_string) - 1u); count++)
            _boot_usb_string[1 + count] = (uint8_t)text[count];
    }

    _boot_usb_string[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * count + 2u));
    return _boot_usb_string;
}

static uint8_t *_boot_usb_put_u16(uint8_t *at, uint16_t value)
{
    at[0] = (uint8_t)(value & 0xFFu);
    at[1] = (uint8_t)(value >> 8);
    return at + 2;
}

static uint8_t *_boot_usb_put_utf16(uint8_t *at, const char *text, uint32_t nuls)
{
    while (*text)
        at = _boot_usb_put_u16(at, (uint8_t)*text++);

    while (nuls--)
        at = _boot_usb_put_u16(at, 0);

    return at;
}

static void _boot_usb_build_ms_os_20(void)
{
    uint8_t *at = _boot_usb_ms_os_20;

    // Set header: length, type, Windows 8.1 and later, total length
    at = _boot_usb_put_u16(at, BOOT_USB_MS_HEADER_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_SET_HEADER_DESCRIPTOR);
    at = _boot_usb_put_u16(at, 0x0000);
    at = _boot_usb_put_u16(at, 0x0603);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_OS_20_LEN);

    // Compatible ID: WinUSB
    at = _boot_usb_put_u16(at, BOOT_USB_MS_COMPAT_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_FEATURE_COMPATBLE_ID);
    memset(at, 0, 16);
    memcpy(at, "WINUSB", 6);
    at += 16;

    // Registry property: DeviceInterfaceGUIDs, a multi-string
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_FEATURE_REG_PROPERTY);
    at = _boot_usb_put_u16(at, 0x0007); // REG_MULTI_SZ
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_NAME_LEN);
    at = _boot_usb_put_utf16(at, BOOT_USB_MS_PROP_NAME, 1);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_DATA_LEN);
    _boot_usb_put_utf16(at, BOOT_USB_INTERFACE_GUID, 2);
}

// --- Bus state ---

void tud_mount_cb(void)
{
    _boot_mounted = true;
}

void tud_umount_cb(void)
{
    _boot_mounted = false;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    _boot_suspended = true;
    _boot_suspended_at_ms = to_ms_since_boot(get_absolute_time());
}

void tud_resume_cb(void)
{
    _boot_suspended = false;
}

// Nothing was written and the host is gone: back to the firmware rather than wait on the battery
static bool _boot_host_gone(uint32_t now_ms)
{
    if (!_boot_mounted)
        return now_ms >= BOOT_USB_UNUSED_MS;

    return _boot_suspended && (now_ms - _boot_suspended_at_ms >= BOOT_USB_SUSPENDED_MS);
}

void boot_usb_run(bool app_valid)
{
    _boot_usb_build_ms_os_20();
    pico_get_unique_board_id_string(_boot_usb_serial, sizeof(_boot_usb_serial));
    memcpy(_boot_first_sector, (const uint8_t *)(XIP_BASE + BOOT_LAYOUT_APP_OFFSET), FLASH_SECTOR_SIZE);

    tud_init(BOOT_USB_RHPORT);
    watchdog_enable(BOOT_USB_WATCHDOG_MS, true);

    for (;;)
    {
        watchdog_update();
        tud_task();

        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if (_boot_restart_pending && ((int32_t)(now_ms - _boot_restart_at_ms) >= 0))
            _boot_restart(_boot_restart_rom);

        if (app_valid && !_boot_dirty && _boot_host_gone(now_ms))
            _boot_restart(false);
    }
}
