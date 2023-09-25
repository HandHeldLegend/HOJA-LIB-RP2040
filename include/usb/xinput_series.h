// Notes for Series Controller USB comms

// 3 interfaces
// Device Descriptor

{
    0x12,       // bLength
    0x01,       // bDescriptorType (Device)
    0x00, 0x02, // bcdUSB 2.00
    0xFF,       // bDeviceClass
    0x47,       // bDeviceSubClass
    0xD0,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0 64
    0x5E, 0x04, // idVendor 0x045E
    0x12, 0x0B, // idProduct 0x0B12
    0x11, 0x05, // bcdDevice 10.11
    0x01,       // iManufacturer (String Index)
    0x02,       // iProduct (String Index)
    0x03,       // iSerialNumber (String Index)
    0x01,       // bNumConfigurations 1
    // 18 bytes
}

// Config Descriptor
{
    0x09,           // bLength
        0x02,       // bDescriptorType (Configuration)
        0x77, 0x00, // wTotalLength 119
        0x03,       // bNumInterfaces 3
        0x01,       // bConfigurationValue
        0x00,       // iConfiguration (String Index)
        0xA0,       // bmAttributes Remote Wakeup
        0xFA,       // bMaxPower 500mA

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x00, // bInterfaceNumber 0
        0x00, // bAlternateSetting
        0x02, // bNumEndpoints 2
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x02,       // bEndpointAddress (OUT/H2D)
        0x03,       // bmAttributes (Interrupt)
        0x40, 0x00, // wMaxPacketSize 64
        0x04,       // bInterval 4 (unit depends on device speed)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x82,       // bEndpointAddress (IN/D2H)
        0x03,       // bmAttributes (Interrupt)
        0x40, 0x00, // wMaxPacketSize 64
        0x04,       // bInterval 4 (unit depends on device speed)

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x00, // bInterfaceNumber 0
        0x01, // bAlternateSetting
        0x02, // bNumEndpoints 2
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x02,       // bEndpointAddress (OUT/H2D)
        0x03,       // bmAttributes (Interrupt)
        0x40, 0x00, // wMaxPacketSize 64
        0x04,       // bInterval 4 (unit depends on device speed)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x82,       // bEndpointAddress (IN/D2H)
        0x03,       // bmAttributes (Interrupt)
        0x40, 0x00, // wMaxPacketSize 64
        0x02,       // bInterval 2 (unit depends on device speed)

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x01, // bInterfaceNumber 1
        0x00, // bAlternateSetting
        0x00, // bNumEndpoints 0
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x01, // bInterfaceNumber 1
        0x01, // bAlternateSetting
        0x02, // bNumEndpoints 2
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x03,       // bEndpointAddress (OUT/H2D)
        0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
        0xE4, 0x00, // wMaxPacketSize 228
        0x01,       // bInterval 1 (unit depends on device speed)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x83,       // bEndpointAddress (IN/D2H)
        0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
        0x40, 0x00, // wMaxPacketSize 64
        0x01,       // bInterval 1 (unit depends on device speed)

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x02, // bInterfaceNumber 2
        0x00, // bAlternateSetting
        0x00, // bNumEndpoints 0
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x09, // bLength
        0x04, // bDescriptorType (Interface)
        0x02, // bInterfaceNumber 2
        0x01, // bAlternateSetting
        0x02, // bNumEndpoints 2
        0xFF, // bInterfaceClass
        0x47, // bInterfaceSubClass
        0xD0, // bInterfaceProtocol
        0x00, // iInterface (String Index)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x04,       // bEndpointAddress (OUT/H2D)
        0x02,       // bmAttributes (Bulk)
        0x40, 0x00, // wMaxPacketSize 64
        0x00,       // bInterval 0 (unit depends on device speed)

        0x07,       // bLength
        0x05,       // bDescriptorType (Endpoint)
        0x84,       // bEndpointAddress (IN/D2H)
        0x02,       // bmAttributes (Bulk)
        0x40, 0x00, // wMaxPacketSize 64
        0x00,       // bInterval 0 (unit depends on device speed)

    // 119 bytes
};

// Connection procedure

/** 
 * 1. After configuration response is received from SET
 * packets of zero length are sent in to endpoint 0x82
 * 
 * 2. A packet of length 32 is sent from the controller to the PC
 * This seems to contain a MAC address, the firmware version, and some other data
 **/

typedef struct
{
    uint8_t report_id; // 0x02
    uint8_t unknown_mac_address_more[15];
    uint8_t version_low[2];     // 0x05, 0x00 LE formatting (5.17.3202.0)
    uint8_t version_middle[2];  // 0x11, 0x00 LE
    uint8_t version_third[2];   // 0x0C, 0x82 LE
    uint8_t version_fourth[2];  // 0x00, 0x00
}   
