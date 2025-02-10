#include "usb/usb.h"
#include "devices/devices.h"
#include "devices/haptics.h"
#include "utilities/interval.h"

#include "bsp/board_api.h"
#include "tusb.h"

#include "hal/mutex_hal.h"
#include "hal/usb_hal.h"
#include "hal/sys_hal.h"

#include "utilities/callback.h"

#include "board_config.h"

#include "usb/ginput_usbd.h"
#include "usb/xinput_usbd.h"
#include "usb/desc_bos.h"
#include "usb/webusb.h"

#include "usb/swpro.h"
#include "usb/xinput.h"
#include "usb/gcinput.h"
#include "usb/opengp.h"

#include "switch/switch_commands.h"
#include "switch/switch_haptics.h"
#include "devices_shared_types.h"

bool _webusb_connected = false;

typedef enum
{
    USBRATE_8 = 8333,
    USBRATE_4 = 4166,
    USBRATE_1 = 500,
} usb_rate_t;

const char* global_string_descriptor[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},                // 0: is supported language is English (0x0409)
    HOJA_MANUFACTURER,              // 1: Manufacturer
    HOJA_PRODUCT,        // 2: Product
    "000000",           // 3: Serials, should use chip ID
    "GC Mode" // 4 Identifier for GC Mode
};

time_callback_t _usb_task_cb = NULL;

uint32_t _usb_clear_owner_0;
uint32_t _usb_clear_owner_0;

// Whether our last input was sent through or not
volatile bool _usb_clear = false;
// Whether USB is ready for another input
volatile bool _usb_ready = false;

MUTEX_HAL_INIT(_hoja_usb_mutex);
void _usb_enter_blocking()
{
  MUTEX_HAL_ENTER_BLOCKING(&_hoja_usb_mutex);
}

bool _usb_enter_try()
{
  if(MUTEX_HAL_ENTER_TIMEOUT_US(&_hoja_usb_mutex, 10))
  {
    return true;
  }
  return false;
}

void _usb_exit()
{
  MUTEX_HAL_EXIT(&_hoja_usb_mutex);
}

void _usb_set_usb_clear()
{
  _usb_enter_blocking();
  _usb_clear = true;
  _usb_exit();
}

void _usb_unset_usb_clear()
{
  _usb_enter_blocking();
  _usb_clear = false;
  _usb_exit();
}

bool _usb_get_usb_clear(uint32_t timestamp)
{
  bool clear = false;

  if(_usb_enter_try())
  {
    clear = _usb_clear;
    _usb_exit();
  }

  static interval_s s = {0};
  if (interval_resettable_run(timestamp, 100000, clear, &s))
  {
    _usb_set_usb_clear();
    return true;
  }

  return clear;
}

// Default 8ms (8000us)
// The rate at which we want to send inputs
uint32_t _usb_rate = 0;
// The minimum number of frames that need to have passed
// before we will initiate a USB data send
uint32_t _usb_frames = 0;

typedef bool (*usb_ready_t)(void);
usb_ready_t _usb_ready_cb = NULL;

// Set up custom TinyUSB XInput Driver
// Sets up custom TinyUSB Device Driver
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
  *driver_count += 1;
  if (hoja_get_status().gamepad_mode == GAMEPAD_MODE_GCUSB)
    return &tud_ginput_driver;
  return &tud_xinput_driver;
}

void _usb_set_interval(usb_rate_t rate)
{
  _usb_rate = rate;
  switch (rate)
  {
  case USBRATE_1:
    _usb_frames = 1;
    break;
  case USBRATE_4:
    _usb_frames = 4;
    break;
  case USBRATE_8:
    _usb_frames = 8;
    break;
  }
}

bool usb_mode_start(gamepad_mode_t mode)
{
  switch (mode)
  {
  default:
  case GAMEPAD_MODE_SWPRO:
    _usb_set_interval(USBRATE_8);
    _usb_task_cb  = swpro_hid_report;
    _usb_ready_cb = tud_hid_ready;
    break;

  case GAMEPAD_MODE_XINPUT:
    _usb_set_interval(USBRATE_1);
    _usb_task_cb  = xinput_hid_report;
    _usb_ready_cb = tud_xinput_ready;
    break;

  case GAMEPAD_MODE_OPENGP:
    _usb_set_interval(USBRATE_1);
    _usb_task_cb  = opengp_hid_report;
    _usb_ready_cb = tud_hid_ready;
    break;

  //case GAMEPAD_MODE_DS4:
  //  imu_set_enabled(true);
  //  _hoja_usb_set_interval(USBRATE_4);
  //  _usb_task_cb = ds4_hid_report;
  //  _usb_ready_cb = tud_hid_ready;
  //  break;

  case GAMEPAD_MODE_GCUSB:
    _usb_set_interval(USBRATE_1);
    _usb_task_cb  = gcinput_hid_report;
    _usb_ready_cb = tud_ginput_ready;
    break;
  }

  board_init();
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);
  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  return true;
}

uint8_t buf = 0;

static inline bool _hoja_usb_ready()
{
  return _usb_ready_cb();
}

void usb_mode_task(uint32_t timestamp)
{
  static interval_s usb_interval  = {0};
  static uint32_t frame_storage   = 0;

  // This flag gets set when we check our frame data
  static bool frame_requirement_met = false;
  static bool usb_requirement_met = false;

  tud_task();

  // Get our SOF frames counter
  uint32_t sof_rd = usb_hal_sof();
  // Handle looparound of the frame counter(11 bits, 2047 max)
  uint32_t dif = (sof_rd > frame_storage) ? (sof_rd - frame_storage) : ((2047 - frame_storage) + sof_rd);

  // Check that we are at the appropriate count of SOF frames
  if (dif >= _usb_frames)
  {
    frame_requirement_met = true;
  }

  // Check that we hit our USB target interval
  if (interval_run(timestamp, _usb_rate, &usb_interval))
  {
    usb_requirement_met = true;
  }

  // Ensure USB is ready
  if (!_usb_ready)
  {
    _usb_ready = _hoja_usb_ready();
  }
  else if (_usb_get_usb_clear(timestamp) && frame_requirement_met && usb_requirement_met)
  {
    frame_storage = sof_rd;
    frame_requirement_met = false;
    usb_requirement_met = false;
    _usb_ready = false;

    _usb_unset_usb_clear();

    _usb_task_cb(timestamp);
  }
}


/***********************************************/
/********* TinyUSB HID callbacks ***************/

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
  switch (hoja_get_status().gamepad_mode)
  {
  default:
  case GAMEPAD_MODE_SWPRO:
    return (uint8_t const *)&swpro_device_descriptor;
    break;

  case GAMEPAD_MODE_XINPUT:
    hoja_set_connected_status(CONN_STATUS_PLAYER_1);
    return (uint8_t const *)&xid_device_descriptor;
    break;

  case GAMEPAD_MODE_GCUSB:
    hoja_set_connected_status(CONN_STATUS_PLAYER_1);
    return (uint8_t const *)&ginput_device_descriptor;
    break;

  case GAMEPAD_MODE_OPENGP:
    hoja_set_connected_status(CONN_STATUS_PLAYER_1);
    return (uint8_t const *)&opengp_device_descriptor;
    break;

  //case GAMEPAD_MODE_DS4:
  //  return (uint8_t const *)&ds4_device_descriptor;
  //  break;
  }
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index; // for multiple configurations
  switch (hoja_get_status().gamepad_mode)
  {
  default:
  case GAMEPAD_MODE_SWPRO:
    return (uint8_t const *)&swpro_configuration_descriptor;
    break;

  case GAMEPAD_MODE_XINPUT:
    return (uint8_t const *)&xid_configuration_descriptor;
    break;

  case GAMEPAD_MODE_GCUSB:
    return (uint8_t const *)&ginput_configuration_descriptor;
    break;

  case GAMEPAD_MODE_OPENGP:
    return (uint8_t const *)&opengp_configuration_descriptor;
    break;

  //case GAMEPAD_MODE_DS4:
  //  return (uint8_t const *)&ds4_configuration_descriptor;
  //  break;
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  (void)instance;
  (void)report_id;
  (void)reqlen;

  return 0;
}

// Invoked when report complete
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
  switch (hoja_get_status().gamepad_mode)
  {
  case GAMEPAD_MODE_SWPRO:
    if ((report[0] == REPORT_ID_SWITCH_INPUT) || (report[0] == REPORT_ID_SWITCH_CMD) || (report[0] == REPORT_ID_SWITCH_INIT))
    {
      _usb_set_usb_clear();
    }
    break;

  default:

  case GAMEPAD_MODE_XINPUT:
    if ((report[0] == REPORT_ID_XINPUT))
    {
      _usb_set_usb_clear();
    }
    break;

  case GAMEPAD_MODE_GCUSB:
    if ((report[0] == REPORT_ID_GAMECUBE))
    {
      _usb_set_usb_clear();
    }
    break;

  case GAMEPAD_MODE_OPENGP:
    if ((report[0] == REPORT_ID_OPENGP))
    {
      _usb_set_usb_clear();
    }
    break;

  //case GAMEPAD_MODE_DS4:
  //  if ((report[0] == REPORT_ID_DS4))
  //  {
  //    hoja_usb_set_usb_clear();
  //  }
  //  break;
  }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  switch (hoja_get_status().gamepad_mode)
  {
  default:
    break;
  case GAMEPAD_MODE_SWPRO:
    if (!report_id && report_type==HID_REPORT_TYPE_OUTPUT)
    {
      if (buffer[0] == SW_OUT_ID_RUMBLE)
      {
        switch_haptics_rumble_translate(&buffer[2]);
      }
      else
      {
        switch_commands_future_handle(buffer[0], buffer, bufsize);
      }
    }
    break;

  case GAMEPAD_MODE_GCUSB:
  if (!report_id && report_type==HID_REPORT_TYPE_OUTPUT)
    {
      if (buffer[0] == 0x11)
      {
        haptics_set_std((buffer[1] & 0x1) ? 235 : 0);
      }
      else if (buffer[0] == 0x13)
      {
        printf("GC INIT CMD RECEIVED");
      }
    }
    break;

  case GAMEPAD_MODE_XINPUT:
    if (!report_id && report_type==HID_REPORT_TYPE_OUTPUT)
    {
      if ((buffer[0] == 0x00) && (buffer[1] == 0x08))
      {
        haptics_set_std((buffer[3] > buffer[4]) ? buffer[3] : buffer[4]);
      }
    }
    break;
  }
}

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void)instance;
  switch (hoja_get_status().gamepad_mode)
  {
  default:
  case GAMEPAD_MODE_SWPRO:
    return swpro_hid_report_descriptor;
    break;

  case GAMEPAD_MODE_GCUSB:
    return gc_hid_report_descriptor;
    break;

  case GAMEPAD_MODE_OPENGP:
    return opengp_hid_report_descriptor;
    break;

  //case GAMEPAD_MODE_DS4:
  //  return ds4_hid_report_descriptor;
  //  break;
  }
  return NULL;
}

static uint16_t _desc_str[64];

// Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
// Define the MS OS 1.0 Descriptor
static const uint8_t MS_OS_Descriptor[] = {
    0x12,                                                                               // Descriptor length (18 bytes)
    0x03,                                                                               // Descriptor type (3 = String)
    0x4D, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, // Signature: "MSFT100"
    VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR,                                                // Vendor Code
    0x00                                                                                // Padding
};

// Size of the uint16_t array
#define SIZE_UINT16_ARRAY (sizeof(MS_OS_Descriptor) / 2)

static uint16_t MS_OS_Descriptor_LE_UINT16[SIZE_UINT16_ARRAY];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid;

  uint8_t chr_count;

  if (index == 0xEE)
  {
    for (int i = 0, j = 0; i < sizeof(MS_OS_Descriptor); i += 2, j++)
    {
      MS_OS_Descriptor_LE_UINT16[j] = (MS_OS_Descriptor[i + 1] << 8) | MS_OS_Descriptor[i];
    }

    memcpy(&_desc_str[0], &MS_OS_Descriptor_LE_UINT16[0], sizeof(MS_OS_Descriptor_LE_UINT16));
    return _desc_str;
  }
  else if (index == 0)
  {
    memcpy(&_desc_str[1], global_string_descriptor[0], 2);
    chr_count = 1;
  }
  else
  {

    const char *str = global_string_descriptor[index];

    // Cap at max char... WHY?
    chr_count = strlen(str);
    if (chr_count > 31)
      chr_count = 31;

    // Convert ASCII string into UTF-16
    for (uint8_t i = 0; i < chr_count; i++)
    {
      _desc_str[1 + i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
  return _desc_str;
}

// Vendor Device Class CB for receiving data
void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
  (void) itf;

  sys_hal_tick();

  uint8_t cmd_buffer[64] = {0};

  uint32_t cmd_read_size = tud_vendor_n_read(0, cmd_buffer, 64);
  tud_vendor_n_read_flush(0);

  if(cmd_read_size>0)
  {
      webusb_command_handler(cmd_buffer, (uint32_t) cmd_read_size);
  }
}

const tusb_desc_webusb_url_t desc_url =
    {
        .bLength = 3 + sizeof(HOJA_WEBUSB_URL) - 1,
        .bDescriptorType = 3, // WEBUSB URL type
        .bScheme = 1,         // 0: http, 1: https
        .url = HOJA_WEBUSB_URL};

uint8_t MS_OS_10_CompatibleID_Descriptor[] = {
    0x28, 0x00, 0x00, 0x00,                         // DWORD (LE)	 Descriptor length (40 bytes)
    0x00, 0x01,                                     // BCD WORD (LE)	 Version ('1.0')
    0x04, 0x00,                                     // WORD (LE)	 Compatibility ID Descriptor index (0x0004)
    0x01,                                           // BYTE	 Number of sections (1)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // 7 BYTES	 Reserved
    0x00,                                           //	 BYTE	 Interface Number (Interface #0)
    0x01,                                           //	 BYTE	 Reserved
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, // 8 BYTES ASCII String Compatible ID ("WINUSB\0\0")
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8 BYTES ASCII String	 Sub-Compatible ID (unused)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00              // 6 BYTES Reserved
};

uint8_t MS_Extended_Feature_Descriptor[] =
    {
        0x92, 0x00, 0x00, 0x00, // DWORD (LE)	 Descriptor length (146 bytes)
        0x00, 0x01,             // BCD WORD (LE) Version ('1.0')
        0x05, 0x00,             // WORD (LE)	 Extended Property Descriptor index (0x0005)
        0x01, 0x00,             // WORD          Number of sections (1)
        0x88, 0x00, 0x00, 0x00, // DWORD (LE)	 Size of the property section (136 bytes)
        0x07, 0x00, 0x00, 0x00, // DWORD (LE)	 Property data type (7 = Unicode REG_MULTI_SZ)
        0x2A, 0x00,             // WORD (LE)	 Property name length (42 bytes)
                                // NULL-terminated Unicode String (LE)	 Property Name (L"DeviceInterfaceGUIDs")
        'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
        'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
        'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0x00, 0x00,
        0x50, 0x00, 0x00, 0x00, // DWORD (LE)	 Property data length (80 bytes)

        // NULL-terminated Unicode String (LE), followed by another Unicode NULL
        // Property Name ("{6E45736A-2B1B-4078-B772-B3AF2B6FDE1C}")
        '{', 0, '6', 0, 'E', 0, '4', 0, '5', 0, '7', 0, '3', 0, '6', 0, 'A', 0, '-', 0,
        '2', 0, 'B', 0, '1', 0, 'B', 0, '-', 0, '4', 0, '0', 0, '7', 0, '8', 0, '-', 0,
        'B', 0, '7', 0, '7', 0, '2', 0, '-', 0, 'B', 0, '3', 0, 'A', 0, 'F', 0, '2', 0,
        'B', 0, '6', 0, 'F', 0, 'D', 0, 'E', 0, '1', 0, 'C', 0, '}', 0,
        0x00, 0x00, 0x00, 0x00};

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP)
    return true;

  uint8_t const desc_type = tu_u16_high(request->wValue);
  uint8_t const itf = 0;

  switch (request->bmRequestType_bit.type)
  {
  case TUSB_REQ_TYPE_STANDARD:
    // Unused for vendor control transfer
    // TinyUSB hooks in and forces this for Vendor requests only

  case TUSB_REQ_TYPE_VENDOR:
    switch (request->bRequest)
    {

    // MS OS 1.0 Descriptor
    case VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR:
    {
      if (request->wIndex == 4)
      {
        if (tud_control_xfer(rhport, request, MS_OS_10_CompatibleID_Descriptor, sizeof(MS_OS_10_CompatibleID_Descriptor)))
        {
          return true;
        }

        return false;
      }
      else if (request->wIndex == 5)
      {
        // MS descriptor 1.0 stuff

        if (tud_control_xfer(rhport, request, MS_Extended_Feature_Descriptor, sizeof(MS_Extended_Feature_Descriptor)))
        {
          return true;
        }
      }
    }
    break;

    // Web USB Descriptor
    case VENDOR_REQUEST_WEBUSB:
    {
      // match vendor request in BOS descriptor
      // Get landing page url
      return tud_control_xfer(rhport, request, (void *)(uintptr_t)&desc_url, desc_url.bLength);
    }

    // MS OS 2.0 Descriptor
    case VENDOR_REQUEST_MICROSOFT:
    {
      if (request->wIndex == 7)
      {
        // Get Microsoft OS 2.0 compatible descriptor
        uint16_t total_len;

        if (hoja_get_status().gamepad_mode == GAMEPAD_MODE_GCUSB)
        {
          memcpy(&total_len, gc_desc_ms_os_20 + 8, 2);
          return tud_control_xfer(rhport, request, (void *)(uintptr_t)gc_desc_ms_os_20, total_len);
        }
        else
        {
          memcpy(&total_len, desc_ms_os_20 + 8, 2);
          return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_20, total_len);
        }
      }
      else
      {
        return false;
      }
    }

    default:
      break;
    }
    break;

  case TUSB_REQ_TYPE_CLASS:
    printf("Vendor Request: %x", request->bRequest);

    // response with status OK
    return tud_control_status(rhport, request);
    break;

  default:
    break;
  }

  // stall unknown request
  return false;
}

/********* USB Data Handling Utilities ***************/
/**
 * @brief Takes in directions and outputs a byte output appropriate for
 * HID Hat usage.
 * @param hat_type hat_mode_t type - The type of controller you're converting for.
 * @param leftRight 0 through 2 (2 is right) to indicate the direction left/right
 * @param upDown 0 through 2 (2 is up) to indicate the direction up/down
 */
uint8_t dir_to_hat(hat_mode_t hat_type, uint8_t leftRight, uint8_t upDown)
{
  uint8_t ret = 0;
  switch (hat_type)
  {
  default:
  case HAT_MODE_DI:
    ret = DI_HAT_CENTER;

    if (leftRight == 2)
    {
      ret = DI_HAT_RIGHT;
      if (upDown == 2)
      {
        ret = DI_HAT_TOP_RIGHT;
      }
      else if (upDown == 0)
      {
        ret = DI_HAT_BOTTOM_RIGHT;
      }
    }
    else if (leftRight == 0)
    {
      ret = DI_HAT_LEFT;
      if (upDown == 2)
      {
        ret = DI_HAT_TOP_LEFT;
      }
      else if (upDown == 0)
      {
        ret = DI_HAT_BOTTOM_LEFT;
      }
    }

    else if (upDown == 2)
    {
      ret = DI_HAT_TOP;
    }
    else if (upDown == 0)
    {
      ret = DI_HAT_BOTTOM;
    }

    return ret;
    break;

  case HAT_MODE_XI:
    ret = XI_HAT_CENTER;

    if (leftRight == 2)
    {
      ret = XI_HAT_RIGHT;
      if (upDown == 2)
      {
        ret = XI_HAT_TOP_RIGHT;
      }
      else if (upDown == 0)
      {
        ret = XI_HAT_BOTTOM_RIGHT;
      }
    }
    else if (leftRight == 0)
    {
      ret = XI_HAT_LEFT;
      if (upDown == 2)
      {
        ret = XI_HAT_TOP_LEFT;
      }
      else if (upDown == 0)
      {
        ret = XI_HAT_BOTTOM_LEFT;
      }
    }

    else if (upDown == 2)
    {
      ret = XI_HAT_TOP;
    }
    else if (upDown == 0)
    {
      ret = XI_HAT_BOTTOM;
    }

    return ret;
    break;
  }
}
