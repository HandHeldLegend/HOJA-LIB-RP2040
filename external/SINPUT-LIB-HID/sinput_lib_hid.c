/*
 * SINPUT-LIB-HID — USB HID report/configuration blobs and device descriptor.
 *
 * Copyright (c) 2026 Hand Held Legend, LLC
 * Author: Mitchell Cairns
 *
 * SPDX-License-Identifier: MIT-0
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "sinput_lib_types.h"

static const uint16_t k_sinput_default_vid = 0x2E8A; // Raspberry Pi
static const uint16_t k_sinput_default_pid = 0x10C6; // Generic SInput/HOJA Gamepad

static const sinput_usb_device_descriptor_t k_sinput_device_descriptor = {
    .bLength = sizeof(sinput_usb_device_descriptor_t),
    .bDescriptorType = 1,
    .bcdUSB = 0x0200, 
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,

    .idVendor = k_sinput_default_vid,
    .idProduct = k_sinput_default_pid,

    .bcdDevice = 0x0210,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
    };

static const uint8_t k_sinput_hid_report_descriptor[139] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                    // Usage (Gamepad)
    0xA1, 0x01,                    // Collection (Application)
    
    // INPUT REPORT ID 0x01 - Main gamepad data
    0x85, 0x01,                    //   Report ID (1)
    
    // Padding bytes (bytes 2-3) - Plug status and Charge Percent (0-100)
    0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined)
    0x09, 0x01,                    //   Usage (Vendor Usage 1)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xFF,                    //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // --- 32 buttons ---
    0x05, 0x09,        // Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x20,        //   Usage Maximum (Button 32)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x01,        //   Logical Max (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    
    // Analog Sticks and Triggers
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    // Left Stick X (bytes 8-9)
    0x09, 0x30,                    //   Usage (X)
    // Left Stick Y (bytes 10-11)
    0x09, 0x31,                    //   Usage (Y)
    // Right Stick X (bytes 12-13)
    0x09, 0x32,                    //   Usage (Z)
    // Right Stick Y (bytes 14-15)
    0x09, 0x35,                    //   Usage (Rz)
    // Right Trigger (bytes 18-19)
    0x09, 0x33,                    //   Usage (Rx)
    // Left Trigger  (bytes 16-17)
    0x09, 0x34,                     //  Usage (Ry)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x06,                    //   Report Count (6)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Padding/Reserved data (bytes 20-63) - 44 bytes
    // This includes gyro/accel data and counter that apps can use if supported
    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined)
    
    // Motion Input Timestamp (Microseconds)
    0x09, 0x20,                    //   Usage (Vendor Usage 0x20)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0xFF,              //   Logical Maximum (655535)
    0x75, 0x20,                    //   Report Size (32)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // Motion Input Accelerometer XYZ (Gs) and Gyroscope XYZ (Degrees Per Second)
    0x09, 0x21,                    //   Usage (Vendor Usage 0x21)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x06,                    //   Report Count (6)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // Reserved padding
    0x09, 0x22,                    //   Usage (Vendor Usage 0x22)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x1D,                    //   Report Count (29)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // INPUT REPORT ID 0x02 - Vendor COMMAND data
    0x85, 0x02,                    //   Report ID (2)
    0x09, 0x23,                    //   Usage (Vendor Usage 0x23)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x3F,                    //   Report Count (63) - 64 bytes minus report ID
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // OUTPUT REPORT ID 0x03 - Vendor COMMAND data
    0x85, 0x03,                    //   Report ID (3)
    0x09, 0x24,                    //   Usage (Vendor Usage 0x24)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x2F,                    //   Report Count (47) - 48 bytes minus report ID
    0x91, 0x02,                    //   Output (Data,Var,Abs)

    0xC0                           // End Collection 
};

static const uint8_t k_sinput_config_descriptor[41] = {
    // === CONFIGURATION DESCRIPTOR ===
    0x09,        // bLength: Descriptor size (9 bytes)
    0x02,        // bDescriptorType: CONFIGURATION (0x02)
    0x29, 0x00,  // wTotalLength: Total length of data returned (41 bytes -> 0x0029)
    0x01,        // bNumInterfaces: 1 interface
    0x01,        // bConfigurationValue: Configuration value
    0x00,        // iConfiguration: Index of string descriptor (None)
    0xA0,        // bmAttributes: Bus Powered, Remote Wakeup
    0xAF,        // bMaxPower: Maximum power consumption (175 * 2mA = 350mA)

    // === INTERFACE DESCRIPTOR ===
    0x09,        // bLength: Descriptor size (9 bytes)
    0x04,        // bDescriptorType: INTERFACE (0x04)
    0x00,        // bInterfaceNumber: Number of this interface (0)
    0x00,        // bAlternateSetting: Alternate setting
    0x02,        // bNumEndpoints: 2 endpoints
    0x03,        // bInterfaceClass: HID (Human Interface Device = 0x03)
    0x00,        // bInterfaceSubClass: None (0=None, 1=Boot)
    0x00,        // bInterfaceProtocol: None (0=None, 1=Keyboard, 2=Mouse)
    0x00,        // iInterface: Index of string descriptor (None)

    // === HID DESCRIPTOR ===
    0x09,        // bLength: Descriptor size (9 bytes)
    0x21,        // bDescriptorType: HID (0x21)
    0x11, 0x01,  // bcdHID: HID Class Spec release number (1.11 -> 0x0111)
    0x00,        // bCountryCode: Hardware target country
    0x01,        // bNumDescriptors: Number of HID class descriptors to follow
    0x22,        // bDescriptorType: REPORT descriptor type (0x22)
    0x8B, 0x00,  // wDescriptorLength: Total length of Report descriptor (139 bytes -> 0x008B)

    // === ENDPOINT DESCRIPTOR (IN) ===
    0x07,        // bLength: Descriptor size (7 bytes)
    0x05,        // bDescriptorType: ENDPOINT (0x05)
    0x81,        // bEndpointAddress: IN Endpoint 1 (0x80 | 0x01)
    0x03,        // bmAttributes: Interrupt transfer type
    0x40, 0x00,  // wMaxPacketSize: 64 bytes max packet size (0x0040)
    0x01,        // bInterval: Polling interval (1ms)

    // === ENDPOINT DESCRIPTOR (OUT) ===
    0x07,        // bLength: Descriptor size (7 bytes)
    0x05,        // bDescriptorType: ENDPOINT (0x05)
    0x01,        // bEndpointAddress: OUT Endpoint 1 (0x00 | 0x01)
    0x03,        // bmAttributes: Interrupt transfer type
    0x40, 0x00,  // wMaxPacketSize: 64 bytes max packet size (0x0040)
    0x04         // bInterval: Polling interval (4ms)
};

const sinput_usb_device_descriptor_t *sinput_hid_get_device_descriptor(void)
{
    return &k_sinput_device_descriptor;
}

void sinput_hid_get_descriptor_params(const uint8_t **hid_report_descriptor, uint16_t *hid_report_descriptor_len,
                                  const uint8_t **configuration_descriptor, uint16_t *configuration_descriptor_len,
                                  uint16_t *vid, uint16_t *pid)
{

    if (vid != NULL)
    {
        *vid = k_sinput_default_vid;
    }
    if (pid != NULL)
    {
        *pid = k_sinput_default_pid;
    }

    if (hid_report_descriptor != NULL)
    {
        *hid_report_descriptor = k_sinput_hid_report_descriptor;
    }
    if (hid_report_descriptor_len != NULL)
    {
        *hid_report_descriptor_len = sizeof(k_sinput_hid_report_descriptor);
    }

    if (configuration_descriptor != NULL)
    {
        *configuration_descriptor = k_sinput_config_descriptor;
    }
    if (configuration_descriptor_len != NULL)
    {
        *configuration_descriptor_len = sizeof(k_sinput_config_descriptor);
    }
}
