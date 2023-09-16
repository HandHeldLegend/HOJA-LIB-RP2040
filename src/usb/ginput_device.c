/*
 * Copyright (c) [2023] [Mitch Cairns/Handheldlegend, LLC]
 * All rights reserved.
 *
 * This source code is licensed under the provisions of the license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ginput_device.h"

bool gc_connected = false;

/**** GameCube Adapter HID Report Descriptor ****/
const uint8_t gc_hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x03,        //   Collection (Report)
    0x85, 0x11,        //     Report ID (17)
    0x19, 0x00,        //     Usage Minimum (Undefined)
    0x2A, 0xFF, 0x00,  //     Usage Maximum (0xFF)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x05,        //     Report Count (5)
    0x91, 0x00,        //     Output (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xA1, 0x03,        //   Collection (Report)
    0x85, 0x21,        //     Report ID (33)
    0x05, 0x00,        //     Usage Page (Undefined)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0xFF,        //     Logical Maximum (-1)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x08,        //     Usage Maximum (0x08)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x33,        //     Usage (Rx)
    0x09, 0x34,        //     Usage (Ry)
    0x09, 0x35,        //     Usage (Rz)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x06,        //     Report Count (6)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xA1, 0x03,        //   Collection (Report)
    0x85, 0x13,        //     Report ID (19)
    0x19, 0x00,        //     Usage Minimum (Undefined)
    0x2A, 0xFF, 0x00,  //     Usage Maximum (0xFF)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x00,        //     Output (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};


/**** GameCube Adapter Device Descriptor ****/
const tusb_desc_device_t ginput_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x057E,
    .idProduct = 0x0337,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

/**** GameCube Adapter Configuration Descriptor ****/
const uint8_t ginput_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 500),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0110), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(gc_hid_report_descriptor)),
    // Endpoint Descriptor
    7,
    TUSB_DESC_ENDPOINT,
    0x82,
    TUSB_XFER_INTERRUPT,
    U16_TO_U8S_LE(37),
    1,

    // Endpoint Descriptor
    7,
    TUSB_DESC_ENDPOINT,
    0x01,
    TUSB_XFER_INTERRUPT,
    U16_TO_U8S_LE(6),
    1,
};

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;

  uint8_t rx_ff_buf[CFG_TUD_GC_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_GC_TX_BUFSIZE];

#if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex;
  osal_mutex_def_t tx_ff_mutex;
#endif

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_GC_RX_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_GC_TX_BUFSIZE];
} ginputd_interface_t;

CFG_TUSB_MEM_SECTION static ginputd_interface_t _ginputd_itf[CFG_TUD_GC];

#define ITF_MEM_RESET_SIZE   offsetof(ginputd_interface_t, rx_ff)


bool tud_ginput_n_mounted (uint8_t itf)
{
  return _ginputd_itf[itf].ep_in && _ginputd_itf[itf].ep_out;
}

uint32_t tud_ginput_n_available (uint8_t itf)
{
  return tu_fifo_count(&_ginputd_itf[itf].rx_ff);
}

bool tud_ginput_n_peek(uint8_t itf, uint8_t* u8)
{
  return tu_fifo_peek(&_ginputd_itf[itf].rx_ff, u8);
}

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+
static void _prep_out_transaction (ginputd_interface_t* p_itf)
{
  uint8_t const rhport = 0;

  // skip if previous transfer not complete
  if ( usbd_edpt_busy(rhport, p_itf->ep_out) ) return;

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  uint16_t max_read = tu_fifo_remaining(&p_itf->rx_ff);
  if ( max_read >= CFG_TUD_GC_RX_BUFSIZE )
  {
    usbd_edpt_xfer(rhport, p_itf->ep_out, p_itf->epout_buf, CFG_TUD_GC_RX_BUFSIZE);
  }
}

uint32_t tud_ginput_n_read (uint8_t itf, void* buffer, uint32_t bufsize)
{
  ginputd_interface_t* p_itf = &_ginputd_itf[itf];
  uint32_t num_read = tu_fifo_read_n(&p_itf->rx_ff, buffer, (uint16_t) bufsize);
  _prep_out_transaction(p_itf);
  return num_read;
}

void tud_ginput_n_read_flush (uint8_t itf)
{
  ginputd_interface_t* p_itf = &_ginputd_itf[itf];
  tu_fifo_clear(&p_itf->rx_ff);
  _prep_out_transaction(p_itf);
}

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+
static uint16_t maybe_transmit(ginputd_interface_t* p_itf)
{
  uint8_t const rhport = 0;

  // skip if previous transfer not complete
  TU_VERIFY( !usbd_edpt_busy(rhport, p_itf->ep_in) );

  uint16_t count = tu_fifo_read_n(&p_itf->tx_ff, p_itf->epin_buf, CFG_TUD_GC_TX_BUFSIZE);
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(rhport, p_itf->ep_in, p_itf->epin_buf, count) );
  }
  return count;
}

uint32_t tud_ginput_n_write (uint8_t itf, void const* buffer, uint32_t bufsize)
{
  ginputd_interface_t* p_itf = &_ginputd_itf[itf];
  uint16_t ret = tu_fifo_write_n(&p_itf->tx_ff, buffer, (uint16_t) bufsize);
  if (tu_fifo_count(&p_itf->tx_ff) >= CFG_TUD_GC_TX_BUFSIZE) {
    maybe_transmit(p_itf);
  }
  return ret;
}

uint32_t tud_ginput_n_flush (uint8_t itf)
{
  ginputd_interface_t* p_itf = &_ginputd_itf[itf];
  uint32_t ret = maybe_transmit(p_itf);

  return ret;
}

uint32_t tud_ginput_n_write_available (uint8_t itf)
{
  return tu_fifo_remaining(&_ginputd_itf[itf].tx_ff);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void ginputd_init(void)
{
  tu_memclr(_ginputd_itf, sizeof(_ginputd_itf));

  for(uint8_t i=0; i<CFG_TUD_GC; i++)
  {
    ginputd_interface_t* p_itf = &_ginputd_itf[i];

    // config fifo
    tu_fifo_config(&p_itf->rx_ff, p_itf->rx_ff_buf, CFG_TUD_GC_RX_BUFSIZE, 1, false);
    tu_fifo_config(&p_itf->tx_ff, p_itf->tx_ff_buf, CFG_TUD_GC_TX_BUFSIZE, 1, false);

#if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&p_itf->rx_ff, NULL, osal_mutex_create(&p_itf->rx_ff_mutex));
    tu_fifo_config_mutex(&p_itf->tx_ff, osal_mutex_create(&p_itf->tx_ff_mutex), NULL);
#endif
  }
}

void ginputd_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_GC; i++)
  {
    ginputd_interface_t* p_itf = &_ginputd_itf[i];

    tu_memclr(p_itf, ITF_MEM_RESET_SIZE);
    tu_fifo_clear(&p_itf->rx_ff);
    tu_fifo_clear(&p_itf->tx_ff);
  }
}

uint16_t ginputd_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
    TU_VERIFY(hoja_comms_current_mode()==INPUT_MODE_GCUSB);

    rgb_set_all(COLOR_BLUE.color);
    rgb_set_dirty();

    uint8_t const * p_desc = tu_desc_next(desc_itf);
    uint8_t const * desc_end = p_desc + max_len;

    // Find available interface
    ginputd_interface_t* p_vendor = NULL;
    for(uint8_t i=0; i<CFG_TUD_GC; i++)
    {
        if ( _ginputd_itf[i].ep_in == 0 && _ginputd_itf[i].ep_out == 0 )
        {
        p_vendor = &_ginputd_itf[i];
        break;
        }
    }
    TU_VERIFY(p_vendor, 0);

    p_vendor->itf_num = desc_itf->bInterfaceNumber;
    if (desc_itf->bNumEndpoints)
    {
        // skip non-endpoint descriptors
        while ( (TUSB_DESC_ENDPOINT != tu_desc_type(p_desc)) && (p_desc < desc_end) )
        {
        p_desc = tu_desc_next(p_desc);
        }

        // Open endpoint pair with usbd helper
        TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &p_vendor->ep_out, &p_vendor->ep_in), 0);

        p_desc += desc_itf->bNumEndpoints*sizeof(tusb_desc_endpoint_t);

        // Prepare for incoming data
        if ( p_vendor->ep_out )
        {
        TU_ASSERT(usbd_edpt_xfer(rhport, p_vendor->ep_out, p_vendor->epout_buf, sizeof(p_vendor->epout_buf)), 0);
        }

        if ( p_vendor->ep_in ) maybe_transmit(p_vendor);
    }
        
    return (uint16_t) ((uintptr_t) p_desc - (uintptr_t) desc_itf);
}

bool ginputd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  uint8_t itf = 0;
  ginputd_interface_t* p_itf = _ginputd_itf;

  for ( ; ; itf++, p_itf++)
  {
    if (itf >= TU_ARRAY_SIZE(_ginputd_itf)) return false;

    if ( ( ep_addr == p_itf->ep_out ) || ( ep_addr == p_itf->ep_in ) ) break;
  }

  if ( ep_addr == p_itf->ep_out )
  {
    // Receive new data
    tu_fifo_write_n(&p_itf->rx_ff, p_itf->epout_buf, (uint16_t) xferred_bytes);

    // Invoked callback if any
    if (tud_vendor_rx_cb) tud_vendor_rx_cb(itf);

    _prep_out_transaction(p_itf);
  }
  else if ( ep_addr == p_itf->ep_in )
  {
    if (tud_vendor_tx_cb) tud_vendor_tx_cb(itf, (uint16_t) xferred_bytes);
    // Send complete, try to send more if possible
    maybe_transmit(p_itf);
  }

  return true;
}

const usbd_class_driver_t tud_ginput_driver =
{
    #if CFG_TUSB_DEBUG >= 2
    .name = "GINPUT",
    #endif
    .init   = ginputd_init,
    .reset  = ginputd_reset,
    .open   = ginputd_open,
    .control_xfer_cb = tud_vendor_control_xfer_cb,
    .xfer_cb    = ginputd_xfer_cb,
    .sof = NULL,
};

bool tud_ginput_ready(void)
{
    uint8_t const rhport = 0;
    uint8_t const ep_in = _ginputd_itf[0].ep_in;
    return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(rhport, ep_in);
}
