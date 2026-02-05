#include <tusb.h>

// string descriptor table
const char *xid_string_descriptor[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "GENERIC",                  // 1: Manufacturer
    "XINPUT CONTROLLER",        // 2: Product
    "1.0"                       // 3: Serials
};

// XINPUT TinyUSB Driver

#define CFG_TUD_XINPUT_EP_BUFSIZE 64

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;        // optional Out endpoint

  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_XINPUT_EP_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_XINPUT_EP_BUFSIZE];

  // TODO save hid descriptor since host can specifically request this after enumeration
  // Note: HID descriptor may be not available from application after enumeration
  //tusb_xinput_descriptor_hid_t const * hid_descriptor;
} xinputd_interface_t;

CFG_TUSB_MEM_SECTION static xinputd_interface_t _xinputd_itf;

void xinputd_init(void)
{
    xinputd_reset(0);
}

void xinputd_reset(uint8_t rhport)
{
    (void) rhport;
    tu_memclr(&_xinputd_itf, sizeof(_xinputd_itf));
}

uint16_t xinputd_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
    const char* TAG = "xinputd_open";
    // Verify our descriptor is the correct class
    TU_VERIFY(0x5D == desc_itf->bInterfaceSubClass, 0);

    // len = interface + hid + n*endpoints
    uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) +
                                        desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t)) + 16;

    TU_ASSERT(max_len >= drv_len, 0);

    uint8_t const * p_desc = tu_desc_next(desc_itf);
    uint8_t total_endpoints = 0;
    while ((total_endpoints < desc_itf->bNumEndpoints) && (drv_len <= max_len) )
    {
        tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
        if (TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep) )
        {
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

            if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN)
            {
                _xinputd_itf.ep_in = desc_ep->bEndpointAddress;
            }
            else
            {
                _xinputd_itf.ep_out = desc_ep->bEndpointAddress;
            }
            total_endpoints += 1;
        }
        p_desc = tu_desc_next(p_desc);
    }

    return drv_len;
}

bool xinputd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
    return true;
}

bool xinputd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  uint8_t instance = 0;

  // Sent report successfully
  if (ep_addr == _xinputd_itf.ep_in)
  {
    if (tud_hid_report_complete_cb)
    {
      tud_hid_report_complete_cb(instance, _xinputd_itf.epin_buf, (uint16_t) xferred_bytes);
    }
  }
  // Received report
  else if (ep_addr == _xinputd_itf.ep_out)
  {
    tud_hid_set_report_cb(instance, 0, HID_REPORT_TYPE_INVALID, _xinputd_itf.epout_buf, (uint16_t) xferred_bytes);
    TU_ASSERT(usbd_edpt_xfer(rhport, _xinputd_itf.ep_out, _xinputd_itf.epout_buf, sizeof(_xinputd_itf.epout_buf)));
  }

  return true;
}

void tud_xinput_getout(void)
{
    if (tud_ready() && (!usbd_edpt_busy(0, _xinputd_itf.ep_out)) )
    {
        usbd_edpt_claim(0, _xinputd_itf.ep_out);
        usbd_edpt_xfer(0, _xinputd_itf.ep_out, _xinputd_itf.epout_buf, sizeof(_xinputd_itf.epout_buf));
        usbd_edpt_release(0, _xinputd_itf.ep_out);
    }
}

// USER API ACCESSIBLE
bool tud_n_xinput_report(uint8_t report_id, void const* report, uint16_t len)
{
    uint8_t const rhport = 0;

    // Remote wakeup
    if (tud_suspended()) {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
    }

    // claim endpoint
    TU_VERIFY( usbd_edpt_claim(rhport, 0x81) );

    _xinputd_itf.epin_buf[0] = report_id;
    memcpy(&_xinputd_itf.epin_buf[1], report, CFG_TUD_XINPUT_EP_BUFSIZE-1);
    bool out = usbd_edpt_xfer(rhport, _xinputd_itf.ep_in, _xinputd_itf.epin_buf, CFG_TUD_XINPUT_EP_BUFSIZE);
    usbd_edpt_release(0, _xinputd_itf.ep_in);

    tud_xinput_getout();
}

bool tud_xinput_report(uint8_t report_id, void const* report, uint16_t len)
{
    return tud_n_xinput_report(report_id, report, len);
}

bool tud_xinput_ready(void)
{
    uint8_t const rhport = 0;
    uint8_t const ep_in = _xinputd_itf.ep_in;
    return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(rhport, ep_in);
}

const usbd_class_driver_t tud_xinput_driver =
{
    #if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
    #endif
    .init   = xinputd_init,
    .reset  = xinputd_reset,
    .open   = xinputd_open,
    .control_xfer_cb = xinputd_control_xfer_cb,
    .xfer_cb    = xinputd_xfer_cb,
    .sof = NULL,
};

// Descriptor callback functions
uint8_t const *xinput_descriptor_device_cb(void)
{
    return (uint8_t const *) &xid_device_descriptor;
}

uint8_t const *xinput_descriptor_configuration_cb(uint8_t index)
{
    (void) index;
    return xid_configuration_descriptor;
}
