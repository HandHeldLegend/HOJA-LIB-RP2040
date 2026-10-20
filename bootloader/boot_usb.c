#include "boot_usb.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

#include "tusb.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"

#include "hal/rp2040/boot_layout.h"
#include "boot_flash.h"
#include "boot_rgb.h"
#include "boot_uf2.h"

#if !defined(BOOT_DEVICE_NAME) || !defined(BOOT_BUILD_ID)
#error "boot_usb.c: hoja_use_bootloader sets BOOT_DEVICE_NAME and BOOT_BUILD_ID for each board"
#endif

// --- The update mode on USB ---

// The HOJA Gamepad IDs, told apart from a running gamepad by the device version (0xB0 in its high
// byte); its serial is the build id (a separate device to Windows, the board to the app), and its
// MS OS 2.0 descriptors bind WinUSB to the update interface. 0xB002 added the UF2 drive beside it.
#define BOOT_USB_VID 0x2E8Au
#define BOOT_USB_PID 0x10C6u
#define BOOT_USB_BCD 0xB002u

// Interface requests beside PICOBOOT's
#define BOOT_USB_REQ_FAULT  0x48u // The last fault: its two scratch registers (BOOT_FAULT_SCRATCH_IDX)
#define BOOT_USB_REQ_LAYOUT 0x49u // boot_usb_layout_report_s

// The layout as the update mode reports it, so tools need no board tables
typedef struct __attribute__((packed))
{
    uint8_t  version;            // BOOT_STAGE_VERSION
    uint8_t  reserved[3];
    uint32_t flash_size;
    uint32_t radio_offset;
    uint32_t radio_size;
    uint32_t app_offset;
    uint32_t stage_header_offset;
    uint32_t top_offset;
} boot_usb_layout_report_s;

_Static_assert(sizeof(boot_usb_layout_report_s) == 28u, "the layout report is a wire format");

#define BOOT_USB_RHPORT         0
#define BOOT_USB_ITF            0
#define BOOT_USB_ITF_DRIVE      1
#define BOOT_USB_EP_OUT         0x01u
#define BOOT_USB_EP_IN          0x81u
#define BOOT_USB_EP_DRIVE_OUT   0x02u
#define BOOT_USB_EP_DRIVE_IN    0x82u
#define BOOT_USB_PACKET_LEN     64u
#define BOOT_USB_VENDOR_MS      0x01u // The MS OS 2.0 vendor request, named in the BOS descriptor
#define BOOT_USB_MS_OS_20_INDEX 7u
#define BOOT_USB_POWER_MA       100u
// A string descriptor in UTF-16 units: its header, then up to 47 characters
#define BOOT_USB_STRING_LEN     48u

// The update interface's own WinUSB device interface GUID (tools find the update mode by it)
#define BOOT_USB_INTERFACE_GUID "{2C8F6F2C-8CEF-4D98-8035-CE6AC66AB70A}"

// With nothing written, the update mode gives way to the firmware once the host is gone: never
// enumerated (the cable came out before the reboot), or suspended (unplugged, or the host asleep)
#define BOOT_USB_UNUSED_MS    10000u
#define BOOT_USB_SUSPENDED_MS 3000u
// A fault in the update mode restarts the chip rather than hang it (on battery nothing else would).
// The loop feeds it, and an erase after each sector: no wait outlasts one sector's erase, or one
// transfer's pages.
#define BOOT_USB_WATCHDOG_MS 2000u
// The LEDs show writing for this long after the last write, then waiting again
#define BOOT_USB_WRITING_SHOWN_MS 1000u
// The acknowledgement leaves before the chip restarts
#define BOOT_USB_REBOOT_MIN_MS 10u
#define BOOT_USB_REBOOT_MAX_MS 2000u

typedef enum
{
    BOOT_USB_STR_LANGUAGE = 0,
    BOOT_USB_STR_MANUFACTURER,
    BOOT_USB_STR_PRODUCT,
    BOOT_USB_STR_SERIAL,
    BOOT_USB_STR_INTERFACE,
} boot_usb_string_t;

// --- PICOBOOT (the RP2040 boot ROM's protocol, as the config app and picotool speak it) ---

#define BOOT_PB_MAGIC              0x431FD10Bu
#define BOOT_PB_CMD_EXCLUSIVE      0x01u
#define BOOT_PB_CMD_REBOOT         0x02u
#define BOOT_PB_CMD_FLASH_ERASE    0x03u
#define BOOT_PB_CMD_WRITE          0x05u
#define BOOT_PB_CMD_EXIT_XIP       0x06u
#define BOOT_PB_CMD_ENTER_XIP      0x07u
#define BOOT_PB_CMD_ROM_BOOTLOADER 0x21u // Ours: restart into the boot ROM's own USB bootloader
#define BOOT_PB_CMD_READ           0x84u
#define BOOT_PB_IF_RESET           0x41u
#define BOOT_PB_IF_CMD_STATUS      0x42u
#define BOOT_PB_CMD_LEN            32u

typedef enum
{
    BOOT_PB_OK = 0,
    BOOT_PB_UNKNOWN_CMD = 1,
    BOOT_PB_INVALID_CMD_LENGTH = 2,
    BOOT_PB_INVALID_TRANSFER_LENGTH = 3,
    BOOT_PB_INVALID_ADDRESS = 4,
    BOOT_PB_BAD_ALIGNMENT = 5,
} boot_pb_status_t;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t token;
    uint8_t  cmd_id;
    uint8_t  cmd_size;
    uint16_t reserved;
    uint32_t transfer_length;
    uint32_t args[4]; // Range: address, size. Reboot: pc, sp, delay in ms.
} boot_pb_cmd_s;

// The command status request's answer
typedef struct __attribute__((packed))
{
    uint32_t token;
    uint32_t status;      // boot_pb_status_t
    uint8_t  cmd_id;
    uint8_t  in_progress; // Never: every command is done before its acknowledgement
    uint8_t  reserved[6];
} boot_pb_status_report_s;

_Static_assert(sizeof(boot_pb_cmd_s) == BOOT_PB_CMD_LEN, "a PICOBOOT command is 32 bytes");
_Static_assert(sizeof(boot_pb_status_report_s) == 16u, "PICOBOOT's command status is 16 bytes");

// --- Descriptors ---

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
    .iManufacturer      = BOOT_USB_STR_MANUFACTURER,
    .iProduct           = BOOT_USB_STR_PRODUCT,
    .iSerialNumber      = BOOT_USB_STR_SERIAL,
    .bNumConfigurations = 1,
};

#define BOOT_USB_CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN + TUD_MSC_DESC_LEN)

static const uint8_t _boot_usb_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, BOOT_USB_CONFIG_LEN, 0x00, BOOT_USB_POWER_MA),
    // The vendor interface as the boot ROM's PICOBOOT one; its name is the build id
    TUD_VENDOR_DESCRIPTOR(BOOT_USB_ITF, BOOT_USB_STR_INTERFACE, BOOT_USB_EP_OUT, BOOT_USB_EP_IN,
                          BOOT_USB_PACKET_LEN),
    // The UF2 drive (boot_uf2.c), for hosts without WebUSB
    TUD_MSC_DESCRIPTOR(BOOT_USB_ITF_DRIVE, 0, BOOT_USB_EP_DRIVE_OUT, BOOT_USB_EP_DRIVE_IN,
                       BOOT_USB_PACKET_LEN),
};

// The MS OS 2.0 set: WinUSB and the interface GUID, for the update interface alone. In a composite
// device they go in a function subset: at the device level WinUSB would take the whole device, and
// Windows would never load the drive.
#define BOOT_USB_MS_HEADER_LEN       0x0Au
#define BOOT_USB_MS_SUBSET_LEN       0x08u // A configuration or function subset header
#define BOOT_USB_MS_WINDOWS_VERSION  0x06030000u // Windows 8.1 and later
#define BOOT_USB_MS_COMPAT_ID        "WINUSB"
#define BOOT_USB_MS_COMPAT_IDS_LEN   16u // The compatible and sub-compatible IDs, 8 bytes each
#define BOOT_USB_MS_COMPAT_LEN       (4u + BOOT_USB_MS_COMPAT_IDS_LEN) // After its length and type
#define BOOT_USB_MS_PROP_FIELDS_LEN  10u // Length, type, data type, name length and data length
#define BOOT_USB_MS_REG_MULTI_SZ     0x0007u
#define BOOT_USB_MS_PROP_NAME        "DeviceInterfaceGUIDs"
#define BOOT_USB_MS_PROP_NAME_LEN    ((sizeof(BOOT_USB_MS_PROP_NAME)) * 2u)         // UTF-16, with its NUL
#define BOOT_USB_MS_PROP_DATA_LEN    ((sizeof(BOOT_USB_INTERFACE_GUID) + 1u) * 2u) // A multi-string: two NULs
#define BOOT_USB_MS_PROP_LEN         (BOOT_USB_MS_PROP_FIELDS_LEN + BOOT_USB_MS_PROP_NAME_LEN + \
                                      BOOT_USB_MS_PROP_DATA_LEN)
#define BOOT_USB_MS_FUNCTION_LEN     (BOOT_USB_MS_SUBSET_LEN + BOOT_USB_MS_COMPAT_LEN + BOOT_USB_MS_PROP_LEN)
#define BOOT_USB_MS_CONFIG_LEN       (BOOT_USB_MS_SUBSET_LEN + BOOT_USB_MS_FUNCTION_LEN)
#define BOOT_USB_MS_OS_20_LEN        (BOOT_USB_MS_HEADER_LEN + BOOT_USB_MS_CONFIG_LEN)

#define BOOT_USB_BOS_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static const uint8_t _boot_usb_bos[] = {
    TUD_BOS_DESCRIPTOR(BOOT_USB_BOS_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(BOOT_USB_MS_OS_20_LEN, BOOT_USB_VENDOR_MS),
};

static uint8_t _boot_usb_ms_os_20[BOOT_USB_MS_OS_20_LEN];
static uint16_t _boot_usb_string[BOOT_USB_STRING_LEN];

// --- State ---

static uint8_t _boot_pb_rhport;
static bool _boot_pb_data_out; // A write's data comes next on the OUT endpoint, not a command
static boot_pb_cmd_s _boot_pb_cmd;
static uint32_t _boot_pb_token;
static boot_pb_status_t _boot_pb_status;
static uint8_t __attribute__((aligned(4))) _boot_pb_packet[BOOT_USB_PACKET_LEN];
static uint8_t __attribute__((aligned(4))) _boot_pb_data[FLASH_SECTOR_SIZE];

// The application slot's first sector, as the host has written it: committed to flash only at the
// restart, so a transfer cut short leaves no vector table and the bootloader stays in update mode
static uint8_t _boot_usb_first_sector[FLASH_SECTOR_SIZE];
static bool _boot_usb_first_sector_written;

static bool _boot_usb_dirty;       // Flash changed: the firmware may be partial, so stay
static uint32_t _boot_usb_written_at_ms;
static bool _boot_usb_mounted;
static bool _boot_usb_suspended;
static uint32_t _boot_usb_suspended_at_ms;
static uint32_t _boot_usb_restart_at_ms;
static bool _boot_usb_restart_pending;
static bool _boot_usb_restart_rom;

// --- Restart ---

static void _boot_usb_commit_first_sector(void)
{
    if (!_boot_usb_first_sector_written)
        return;

    boot_flash_write_sector(BOOT_LAYOUT_APP_OFFSET, _boot_usb_first_sector);
    _boot_usb_first_sector_written = false;
}

static void __attribute__((noreturn)) _boot_usb_restart(bool rom)
{
    _boot_usb_commit_first_sector();
    tud_disconnect();

    if (rom)
    {
        boot_rgb_off();
        reset_usb_boot(0, 0);
    }

    watchdog_reboot(0, 0, 0);
    for (;;)
        tight_loop_contents();
}

static void _boot_usb_schedule_restart(uint32_t delay_ms, bool rom)
{
    delay_ms = MAX(delay_ms, BOOT_USB_REBOOT_MIN_MS);
    delay_ms = MIN(delay_ms, BOOT_USB_REBOOT_MAX_MS);
    _boot_usb_restart_at_ms = to_ms_since_boot(get_absolute_time()) + delay_ms;
    _boot_usb_restart_rom = rom;
    _boot_usb_restart_pending = true;
}

// --- Flash access through the PICOBOOT commands ---

// The bootloader never rewrites itself (it runs from there): writes to it are acknowledged and
// dropped, so an image that carries it can be written whole
static bool _boot_usb_in_bootloader(uint32_t offset)
{
    return offset < BOOT_LAYOUT_BOOT_SIZE;
}

static bool _boot_usb_in_first_sector(uint32_t offset)
{
    return (offset >= BOOT_LAYOUT_APP_OFFSET) && (offset < BOOT_LAYOUT_APP_OFFSET + FLASH_SECTOR_SIZE);
}

static bool _boot_usb_range_valid(uint32_t addr, uint32_t len, uint32_t *offset)
{
    if ((addr < XIP_BASE) || (len > PICO_FLASH_SIZE_BYTES) ||
        (addr - XIP_BASE > PICO_FLASH_SIZE_BYTES - len))
        return false;

    *offset = addr - XIP_BASE;
    return true;
}

// A sector at a time, the watchdog fed after each: a whole image's range would outlast it
void boot_usb_erase_sector(uint32_t offset)
{
    if (_boot_usb_in_bootloader(offset))
        return;

    if (_boot_usb_in_first_sector(offset))
    {
        memset(_boot_usb_first_sector, 0xFF, sizeof(_boot_usb_first_sector));
        _boot_usb_first_sector_written = false;
    }

    boot_flash_erase(offset, FLASH_SECTOR_SIZE);
    watchdog_update();
    _boot_usb_dirty = true;
    _boot_usb_written_at_ms = to_ms_since_boot(get_absolute_time());
}

static boot_pb_status_t _boot_usb_erase(uint32_t addr, uint32_t len)
{
    uint32_t offset;

    if (!_boot_usb_range_valid(addr, len, &offset))
        return BOOT_PB_INVALID_ADDRESS;

    if ((offset % FLASH_SECTOR_SIZE) || (len % FLASH_SECTOR_SIZE))
        return BOOT_PB_BAD_ALIGNMENT;

    for (uint32_t at = offset; at < offset + len; at += FLASH_SECTOR_SIZE)
        boot_usb_erase_sector(at);
    return BOOT_PB_OK;
}

// The range is checked and page aligned (_boot_pb_start_data_out)
static void _boot_usb_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    for (uint32_t done = 0; done < len; done += FLASH_PAGE_SIZE)
    {
        uint32_t at = offset + done;

        if (_boot_usb_in_bootloader(at))
            continue;

        if (_boot_usb_in_first_sector(at))
        {
            // Programming only clears bits, as it would on the flash itself
            uint8_t *page = &_boot_usb_first_sector[at - BOOT_LAYOUT_APP_OFFSET];
            for (uint32_t i = 0; i < FLASH_PAGE_SIZE; i++)
                page[i] &= data[done + i];
            _boot_usb_first_sector_written = true;
        }
        else
        {
            boot_flash_program(at, &data[done], FLASH_PAGE_SIZE);
        }
        _boot_usb_dirty = true;
        _boot_usb_written_at_ms = to_ms_since_boot(get_absolute_time());
    }
}

void boot_usb_program_page(uint32_t offset, const uint8_t *data)
{
    _boot_usb_write(offset, data, FLASH_PAGE_SIZE);
}

void boot_usb_restart_after(uint32_t delay_ms)
{
    _boot_usb_schedule_restart(delay_ms, false);
}

static void _boot_usb_read(uint32_t offset, uint8_t *out, uint32_t len)
{
    memcpy(out, (const uint8_t *)(XIP_BASE + offset), len);

    // The first sector reads back as written, though not yet committed
    for (uint32_t i = 0; i < len; i++)
    {
        if (_boot_usb_in_first_sector(offset + i))
            out[i] = _boot_usb_first_sector[offset + i - BOOT_LAYOUT_APP_OFFSET];
    }
}

// --- PICOBOOT transfers ---

static void _boot_pb_arm_command(void)
{
    _boot_pb_data_out = false;
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
    _boot_pb_data_out = false;
    usbd_edpt_xfer(_boot_pb_rhport, BOOT_USB_EP_IN, NULL, 0);
}

// The host reads the reason with the command status request, and clears it with the reset request
static void _boot_pb_fail(boot_pb_status_t status)
{
    _boot_pb_status = status;
    _boot_pb_data_out = false;
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

    if (!_boot_usb_range_valid(_boot_pb_cmd.args[0], len, &offset))
    {
        _boot_pb_fail(BOOT_PB_INVALID_ADDRESS);
        return;
    }

    if ((offset % FLASH_PAGE_SIZE) || (len % FLASH_PAGE_SIZE))
    {
        _boot_pb_fail(BOOT_PB_BAD_ALIGNMENT);
        return;
    }

    _boot_pb_data_out = true;
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

    if (!_boot_usb_range_valid(_boot_pb_cmd.args[0], len, &offset))
    {
        _boot_pb_fail(BOOT_PB_INVALID_ADDRESS);
        return;
    }

    _boot_usb_read(offset, _boot_pb_data, len);
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
            _boot_usb_schedule_restart(_boot_pb_cmd.args[2], false);
            _boot_pb_finish(BOOT_PB_OK);
            break;

        case BOOT_PB_CMD_ROM_BOOTLOADER:
            _boot_usb_schedule_restart(0, true);
            _boot_pb_finish(BOOT_PB_OK);
            break;

        case BOOT_PB_CMD_FLASH_ERASE:
            _boot_pb_finish(_boot_usb_erase(_boot_pb_cmd.args[0], _boot_pb_cmd.args[1]));
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
    if (!_boot_pb_data_out)
    {
        // An empty packet is the host's acknowledgement of a read
        if (len == 0)
            _boot_pb_arm_command();
        else
            _boot_pb_command(len);
        return;
    }

    if (len != _boot_pb_cmd.transfer_length)
    {
        _boot_pb_fail(BOOT_PB_INVALID_TRANSFER_LENGTH);
        return;
    }
    _boot_usb_write(_boot_pb_cmd.args[0] - XIP_BASE, _boot_pb_data, len);
    _boot_pb_finish(BOOT_PB_OK);
}

static void _boot_pb_in_done(void)
{
    // A read's data is out (the host's empty acknowledgement follows), or our acknowledgement is
    _boot_pb_arm_command();
}

// --- Class driver ---

static void _boot_pb_init(void)
{
    _boot_pb_data_out = false;
}

static void _boot_pb_reset(uint8_t rhport)
{
    (void)rhport;
    _boot_pb_data_out = false;
}

static uint16_t _boot_pb_open(uint8_t rhport, tusb_desc_interface_t const *desc, uint16_t max_len)
{
    if ((desc->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC) || (max_len < TUD_VENDOR_DESC_LEN))
        return 0;

    uint8_t ep_out;
    uint8_t ep_in;
    const uint8_t *ep_desc = tu_desc_next(desc);
    if (!usbd_open_edpt_pair(rhport, ep_desc, 2, TUSB_XFER_BULK, &ep_out, &ep_in))
        return 0;

    _boot_pb_rhport = rhport;
    _boot_pb_arm_command();
    return TUD_VENDOR_DESC_LEN;
}

// Standard requests to its endpoints come here once TinyUSB has acted on them, and are acknowledged
// whatever this returns. Clearing a halt, as Windows does when it resets a pipe, leaves the endpoint
// idle: wait for a command again. Anything else declines, so TinyUSB answers the interface's
// standard requests itself and stalls the rest.
static bool _boot_pb_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    (void)rhport;

    if ((request->bmRequestType_bit.type != TUSB_REQ_TYPE_STANDARD) ||
        (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_ENDPOINT))
        return false;

    if ((stage == CONTROL_STAGE_SETUP) && (request->bRequest == TUSB_REQ_CLEAR_FEATURE) &&
        (request->wValue == TUSB_REQ_FEATURE_EDPT_HALT))
    {
        // Whatever was under way on either endpoint is over: wait for a command
        _boot_pb_endpoint_restart(tu_u16_low(request->wIndex));
        _boot_pb_arm_command();
    }
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

// TinyUSB's own drivers come after it: the drive's interface goes to its MSC driver
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &_boot_pb_driver;
}

// --- Control requests ---

static bool _boot_pb_interface_request(uint8_t rhport, tusb_control_request_t const *request)
{
    // Held until the transfer is done
    static boot_pb_status_report_s status;
    static uint32_t fault[2];

    switch (request->bRequest)
    {
        case BOOT_PB_IF_RESET:
            _boot_pb_endpoint_restart(BOOT_USB_EP_OUT);
            _boot_pb_endpoint_restart(BOOT_USB_EP_IN);
            _boot_pb_status = BOOT_PB_OK;
            _boot_pb_arm_command();
            return tud_control_status(rhport, request);

        case BOOT_USB_REQ_LAYOUT:
        {
            static const boot_usb_layout_report_s layout = {
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

        case BOOT_USB_REQ_FAULT:
            fault[0] = watchdog_hw->scratch[BOOT_FAULT_SCRATCH_IDX];
            fault[1] = watchdog_hw->scratch[BOOT_FAULT_SCRATCH_IDX + 1];
            return tud_control_xfer(rhport, request, fault, sizeof(fault));

        case BOOT_PB_IF_CMD_STATUS:
            status = (boot_pb_status_report_s){
                .token = _boot_pb_token,
                .status = _boot_pb_status,
                .cmd_id = _boot_pb_cmd.cmd_id,
            };
            return tud_control_xfer(rhport, request, &status, sizeof(status));

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

// --- Descriptor callbacks ---

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
        case BOOT_USB_STR_LANGUAGE:
            _boot_usb_string[1] = 0x0409; // English (US)
            count = 1;
            break;

        case BOOT_USB_STR_MANUFACTURER:
            text = "Hand Held Legend";
            break;

        case BOOT_USB_STR_PRODUCT:
            text = BOOT_DEVICE_NAME " Update";
            break;

        case BOOT_USB_STR_SERIAL:
        case BOOT_USB_STR_INTERFACE:
            // The build id: the app knows the board before opening it, and the browser's permission
            // covers every controller of the kind
            text = BOOT_BUILD_ID;
            break;

        default:
            return NULL;
    }

    if (index != BOOT_USB_STR_LANGUAGE)
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

static uint8_t *_boot_usb_put_u32(uint8_t *at, uint32_t value)
{
    at = _boot_usb_put_u16(at, (uint16_t)(value & 0xFFFFu));
    return _boot_usb_put_u16(at, (uint16_t)(value >> 16));
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

    // Set header: length, type, Windows version, total length
    at = _boot_usb_put_u16(at, BOOT_USB_MS_HEADER_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_SET_HEADER_DESCRIPTOR);
    at = _boot_usb_put_u32(at, BOOT_USB_MS_WINDOWS_VERSION);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_OS_20_LEN);

    // The only configuration (index 0), and in it the update interface's function
    at = _boot_usb_put_u16(at, BOOT_USB_MS_SUBSET_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_SUBSET_HEADER_CONFIGURATION);
    *at++ = 0;
    *at++ = 0;
    at = _boot_usb_put_u16(at, BOOT_USB_MS_CONFIG_LEN);

    at = _boot_usb_put_u16(at, BOOT_USB_MS_SUBSET_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_SUBSET_HEADER_FUNCTION);
    *at++ = BOOT_USB_ITF;
    *at++ = 0;
    at = _boot_usb_put_u16(at, BOOT_USB_MS_FUNCTION_LEN);

    // Compatible ID: WinUSB, NUL padded, and no sub-compatible ID
    at = _boot_usb_put_u16(at, BOOT_USB_MS_COMPAT_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_FEATURE_COMPATBLE_ID);
    memset(at, 0, BOOT_USB_MS_COMPAT_IDS_LEN);
    memcpy(at, BOOT_USB_MS_COMPAT_ID, sizeof(BOOT_USB_MS_COMPAT_ID) - 1u);
    at += BOOT_USB_MS_COMPAT_IDS_LEN;

    // Registry property: DeviceInterfaceGUIDs, a multi-string
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_LEN);
    at = _boot_usb_put_u16(at, MS_OS_20_FEATURE_REG_PROPERTY);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_REG_MULTI_SZ);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_NAME_LEN);
    at = _boot_usb_put_utf16(at, BOOT_USB_MS_PROP_NAME, 1);
    at = _boot_usb_put_u16(at, BOOT_USB_MS_PROP_DATA_LEN);
    _boot_usb_put_utf16(at, BOOT_USB_INTERFACE_GUID, 2);
}

// --- Bus state ---

void tud_mount_cb(void)
{
    _boot_usb_mounted = true;
}

void tud_umount_cb(void)
{
    _boot_usb_mounted = false;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    _boot_usb_suspended = true;
    _boot_usb_suspended_at_ms = to_ms_since_boot(get_absolute_time());
}

void tud_resume_cb(void)
{
    _boot_usb_suspended = false;
}

// Nothing was written and the host is gone: back to the firmware rather than wait on the battery
static bool _boot_usb_host_gone(uint32_t now_ms)
{
    if (!_boot_usb_mounted)
        return now_ms >= BOOT_USB_UNUSED_MS;

    return _boot_usb_suspended && (now_ms - _boot_usb_suspended_at_ms >= BOOT_USB_SUSPENDED_MS);
}

void boot_usb_run(bool app_valid)
{
    _boot_usb_build_ms_os_20();
    memcpy(_boot_usb_first_sector, (const uint8_t *)(XIP_BASE + BOOT_LAYOUT_APP_OFFSET), FLASH_SECTOR_SIZE);

    tud_init(BOOT_USB_RHPORT);
    watchdog_enable(BOOT_USB_WATCHDOG_MS, true);

    for (;;)
    {
        watchdog_update();
        tud_task();

        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        bool writing = _boot_usb_dirty && (now_ms - _boot_usb_written_at_ms < BOOT_USB_WRITING_SHOWN_MS);
        if (boot_uf2_refused(now_ms))
            boot_rgb_task(BOOT_RGB_REFUSED);
        else
            boot_rgb_task(writing ? BOOT_RGB_WRITING : BOOT_RGB_UPDATE_MODE);

        if (_boot_usb_restart_pending && ((int32_t)(now_ms - _boot_usb_restart_at_ms) >= 0))
            _boot_usb_restart(_boot_usb_restart_rom);

        if (app_valid && !_boot_usb_dirty && _boot_usb_host_gone(now_ms))
            _boot_usb_restart(false);
    }
}
