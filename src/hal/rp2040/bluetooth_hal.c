#include "hal/bluetooth_hal.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)

#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack_config.h"

#include "btstack.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"

#include <string.h>

#include "switch/switch_commands.h"
#include "usb/swpro.h"
#include "hal/sys_hal.h"

volatile bool _connected = false;

const uint8_t procon_hid_descriptor[213] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                   // Usage (Game Pad)
    0xA1, 0x01,                   // Collection (Application)
    0x85, 0x21,                   //   Report ID (33)
    0x06, 0x01, 0xFF,             //   Usage Page (Vendor Defined 0xFF01)
    0x09, 0x21,                   //   Usage (0x21)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x30,                   //   Report ID (48)
    0x09, 0x30,                   //   Usage (0x30)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x31,                   //   Report ID (49)
    0x09, 0x31,                   //   Usage (0x31)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x32,                   //   Report ID (50)
    0x09, 0x32,                   //   Usage (0x32)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x33,                   //   Report ID (51)
    0x09, 0x33,                   //   Usage (0x33)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x3F,                   //   Report ID (63)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (0x01)
    0x29, 0x10,                   //   Usage Maximum (0x10)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x10,                   //   Report Count (16)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,                   //   Usage (Hat switch)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x42,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x30,                   //   Usage (X)
    0x09, 0x31,                   //   Usage (Y)
    0x09, 0x33,                   //   Usage (Rx)
    0x09, 0x34,                   //   Usage (Ry)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x10,                   //   Report Size (16)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x01,                   //   Report ID (1)
    0x06, 0x01, 0xFF,             //   Usage Page (Vendor Defined 0xFF01)
    0x09, 0x01,                   //   Usage (0x01)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x10,                   //   Report ID (16)
    0x09, 0x10,                   //   Usage (0x10)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x11,                   //   Report ID (17)
    0x09, 0x11,                   //   Usage (0x11)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x12,                   //   Report ID (18)
    0x09, 0x12,                   //   Usage (0x12)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0                          // End Collection
};

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[700];
static uint8_t pnp_service_buffer[200];
static btstack_packet_callback_registration_t hci_event_callback_registration;
static int hid_cid = 0;

void _create_sdp_pnp_record(uint8_t *service, uint16_t vendor_id_source,
                           uint16_t vendor_id, uint16_t product_id,
                           uint16_t version)
{
    uint8_t *attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10001);

    // 0x0001 "Service Class ID List"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16,
                      BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol, DE_UUID, DE_SIZE_16,
                          BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol, DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_SDP);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t *sdpProtocol = de_push_sequence(attribute);
        {
            de_add_number(sdpProtocol, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_SDP);
        }
        de_pop_sequence(attribute, sdpProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0006 "Language Base Attribute ID List"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *pnpProfile = de_push_sequence(attribute);
        {
            de_add_number(pnpProfile, DE_UUID, DE_SIZE_16,
                          BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
            de_add_number(pnpProfile, DE_UINT, DE_SIZE_16, 0x0100); // Version 1.1
        }
        de_pop_sequence(attribute, pnpProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "ServiceName"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service, DE_STRING,
                (uint16_t)strlen("Wireless Gamepad PnP Server"),
                (uint8_t *)"Wireless Gamepad PnP Server");

    // 0x0101 "ServiceDescription"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0101);
    de_add_data(service, DE_STRING, (uint16_t)strlen("Gamepad"),
                (uint8_t *)"Gamepad");

    // 0x0200 "SpecificationID"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_SPECIFICATION_ID);
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0103); // v1.03

    // 0x0201 "VendorID"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_VENDOR_ID);
    de_add_number(service, DE_UINT, DE_SIZE_16, vendor_id);

    // 0x0202 "ProductID"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PRODUCT_ID);
    de_add_number(service, DE_UINT, DE_SIZE_16, product_id);

    // 0x0203 "Version"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_VERSION);
    de_add_number(service, DE_UINT, DE_SIZE_16, version);

    // 0x0204 "PrimaryRecord"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_PRIMARY_RECORD);
    de_add_number(service, DE_BOOL, DE_SIZE_8,
                  1); // yes, this is the primary record - there are no others

    // 0x0205 "VendorIDSource"
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_VENDOR_ID_SOURCE);
    de_add_number(service, DE_UINT, DE_SIZE_16, vendor_id_source);
}

void create_sdp_hid_record(uint8_t *service, const hid_sdp_record_t *params) {
    uint8_t *attribute;
    de_create_sequence(service);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10000);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
      de_add_number(attribute, DE_UUID, DE_SIZE_16,
                    BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
      uint8_t *l2cpProtocol = de_push_sequence(attribute);
      {
        de_add_number(l2cpProtocol, DE_UUID, DE_SIZE_16,
                      BLUETOOTH_PROTOCOL_L2CAP);
        de_add_number(l2cpProtocol, DE_UINT, DE_SIZE_16,
                      BLUETOOTH_PSM_HID_CONTROL);
      }
      de_pop_sequence(attribute, l2cpProtocol);
  
      uint8_t *hidProtocol = de_push_sequence(attribute);
      {
        de_add_number(hidProtocol, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_HIDP);
      }
      de_pop_sequence(attribute, hidProtocol);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST);
    attribute = de_push_sequence(service);
    {
      de_add_number(attribute, DE_UUID, DE_SIZE_16,
                    BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
    attribute = de_push_sequence(service);
    {
      de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
      de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
      de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
      uint8_t *hidProfile = de_push_sequence(attribute);
      {
        de_add_number(hidProfile, DE_UUID, DE_SIZE_16,
                      BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
        de_add_number(hidProfile, DE_UINT, DE_SIZE_16, 0x0101);  // Version 1.1
      }
      de_pop_sequence(attribute, hidProfile);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS);
    attribute = de_push_sequence(service);
    {
      uint8_t *additionalDescriptorAttribute = de_push_sequence(attribute);
      {
        uint8_t *l2cpProtocol = de_push_sequence(additionalDescriptorAttribute);
        {
          de_add_number(l2cpProtocol, DE_UUID, DE_SIZE_16,
                        BLUETOOTH_PROTOCOL_L2CAP);
          de_add_number(l2cpProtocol, DE_UINT, DE_SIZE_16,
                        BLUETOOTH_PSM_HID_INTERRUPT);
        }
        de_pop_sequence(additionalDescriptorAttribute, l2cpProtocol);
  
        uint8_t *hidProtocol = de_push_sequence(additionalDescriptorAttribute);
        {
          de_add_number(hidProtocol, DE_UUID, DE_SIZE_16,
                        BLUETOOTH_PROTOCOL_HIDP);
        }
        de_pop_sequence(additionalDescriptorAttribute, hidProtocol);
      }
      de_pop_sequence(attribute, additionalDescriptorAttribute);
    }
    de_pop_sequence(service, attribute);
  
    // 0x0100 "ServiceName"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service, DE_STRING, (uint16_t)strlen(params->device_name),
                (uint8_t *)params->device_name);
  
    // 0x0101 "ServiceDescription"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0101);
    de_add_data(service, DE_STRING, (uint16_t)strlen("Gamepad"),
                (uint8_t *)"Gamepad");
  
    // 0x0102 "ProviderName"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0102);
    de_add_data(service, DE_STRING, (uint16_t)strlen("Nintendo"),
                (uint8_t *)"Nintendo");
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_PARSER_VERSION);
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0111);  // v1.1.1
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_DEVICE_SUBCLASS);
    de_add_number(service, DE_UINT, DE_SIZE_8, params->hid_device_subclass);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_COUNTRY_CODE);
    de_add_number(service, DE_UINT, DE_SIZE_8, params->hid_country_code);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_VIRTUAL_CABLE);
    de_add_number(service, DE_BOOL, DE_SIZE_8, params->hid_virtual_cable);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_RECONNECT_INITIATE);
    de_add_number(service, DE_BOOL, DE_SIZE_8, params->hid_reconnect_initiate);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
      uint8_t *hidDescriptor = de_push_sequence(attribute);
      {
        de_add_number(hidDescriptor, DE_UINT, DE_SIZE_8,
                      0x22);  // Report Descriptor
        de_add_data(hidDescriptor, DE_STRING, params->hid_descriptor_size,
                    (uint8_t *)params->hid_descriptor);
      }
      de_pop_sequence(attribute, hidDescriptor);
    }
    de_pop_sequence(service, attribute);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HIDLANGID_BASE_LIST);
    attribute = de_push_sequence(service);
    {
      uint8_t *hig_lang_base = de_push_sequence(attribute);
      {
        // see: http://www.usb.org/developers/docs/USB_LANGIDs.pdf
        de_add_number(hig_lang_base, DE_UINT, DE_SIZE_16,
                      0x0409);  // HIDLANGID = English (US)
        de_add_number(hig_lang_base, DE_UINT, DE_SIZE_16,
                      0x0100);  // HIDLanguageBase = 0x0100 default
      }
      de_pop_sequence(attribute, hig_lang_base);
    }
    de_pop_sequence(service, attribute);
  
    // battery power
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_BATTERY_POWER);
    de_add_number(service, DE_BOOL, DE_SIZE_8, 1);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_REMOTE_WAKE);
    de_add_number(service, DE_BOOL, DE_SIZE_8, params->hid_remote_wake ? 1 : 0);
  
    // supervision timeout
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_SUPERVISION_TIMEOUT);
    de_add_number(service, DE_UINT, DE_SIZE_16, params->hid_supervision_timeout);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_NORMALLY_CONNECTABLE);
    de_add_number(service, DE_BOOL, DE_SIZE_8, params->hid_normally_connectable);
  
    de_add_number(service, DE_UINT, DE_SIZE_16,
                  BLUETOOTH_ATTRIBUTE_HID_BOOT_DEVICE);
    de_add_number(service, DE_BOOL, DE_SIZE_8, params->hid_boot_device ? 1 : 0);
  }

bool _bluetooth_hal_hid_tunnel(uint8_t report_id, const void *report, uint16_t len)
{
    static uint8_t new_report[64] = {0};
    new_report[0] = 0xA1; // Type of input report
    new_report[1] = report_id;
    memcpy(&(new_report[2]), report, len);

    if (hid_cid)
        hid_device_send_interrupt_message(hid_cid, new_report, 48);

    return true;
}

static void _bt_hid_report_handler(uint16_t cid,
                                   hid_report_type_t report_type,
                                   uint16_t report_id,
                                   int report_size, uint8_t *report)
{
    if(report_type == HID_REPORT_TYPE_OUTPUT)
    {
        if (cid == hid_cid)
        {
            printf("REPORT? %x, c: %d\n", report_id, hid_cid);
            uint8_t tmp[64] = {0};

            tmp[0] = report_id;
            
            if (report_id == SW_OUT_ID_RUMBLE)
            {
                switch_haptics_rumble_translate(&report[2]);
            }
            else if (report_id == SW_OUT_ID_RUMBLE_CMD)
            {
                memcpy(&tmp[1], report, report_size);
                switch_commands_future_handle(report_id, tmp, report_size+1);
            }
        }
    }
}

static void _bt_hal_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t packet_size)
{
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;
    if (packet_type == HCI_EVENT_PACKET)
    {
        switch (packet[0])
        {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
                return;
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            // ssp: inform about user confirmation request
            printf("SSP User Confirmation Auto accept\n");
            break;
        case HCI_EVENT_HID_META:
            switch (hci_event_hid_meta_get_subevent_code(packet))
            {
            case HID_SUBEVENT_CONNECTION_OPENED:
                status = hid_subevent_connection_opened_get_status(packet);
                if (status)
                {
                    // outgoing connection failed
                    printf("Connection failed, status 0x%x\n", status);
                    _connected = false;
                    hid_cid = 0;
                    return;
                }
                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                printf("HID Connected\n");
                _connected = true;
                hid_device_request_can_send_now_event(hid_cid);
                break;
            case HID_SUBEVENT_CONNECTION_CLOSED:
                printf("HID Disconnected\n");
                _connected = false;
                hid_cid = 0;
                break;
            case HID_SUBEVENT_CAN_SEND_NOW:
                if (hid_cid != 0)
                { // Solves crash when disconnecting gamepad on android
                    swpro_hid_report(0, _bluetooth_hal_hid_tunnel);
                    hid_device_request_can_send_now_event(hid_cid);
                }
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

bool bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb)
{
    // If the init fails it returns true lol
    if (cyw43_arch_init())
    {
        return false;
    }
    
    bd_addr_t newAddr = {0x7c,
                         0xbb,
                         0x8a,
                         (uint8_t)(get_rand_32() % 0xff),
                         (uint8_t)(get_rand_32() % 0xff),
                         (uint8_t)(get_rand_32() % 0xff)};

    gap_discoverable_control(1);
    gap_set_class_of_device(0x2508);
    gap_set_local_name("Pro Controller");
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH |
                                         LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    hci_set_chipset(btstack_chipset_cyw43_instance());
    hci_set_bd_addr(newAddr);

    // L2CAP
    l2cap_init();
    sm_init();
    // SDP Server
    sdp_init();

    hid_sdp_record_t hid_sdp_record = {0x2508,
                                       33,
                                       1,
                                       1,
                                       1,
                                       0,
                                       0,
                                       0xFFFF,
                                       0xFFFF,
                                       3200,
                                       procon_hid_descriptor,
                                       213,
                                       hid_device_name};

    // Register SDP services
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
    create_sdp_hid_record(hid_service_buffer, &hid_sdp_record);
    sdp_register_service(hid_service_buffer);


    memset(pnp_service_buffer, 0, sizeof(pnp_service_buffer));
    _create_sdp_pnp_record(pnp_service_buffer, DEVICE_ID_VENDOR_ID_SOURCE_USB,
                          0x057E, 0x2009, 0x0001);
    sdp_register_service(pnp_service_buffer);

    // HID Device
    hid_device_init(1, 213,
        procon_hid_descriptor);

    hci_event_callback_registration.callback = &_bt_hal_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_report_data_callback(&_bt_hid_report_handler);

    hci_power_control(HCI_POWER_ON);

    //btstack_run_loop_execute();

    return true;
}

void bluetooth_hal_stop()
{
}


void bluetooth_hal_task(uint32_t timestamp)
{
    sleep_ms(1);
}

uint32_t bluetooth_hal_get_fwversion()
{
}

#endif