#include "usb/ds4input.h"
#include "input_shared_types.h"
#include "usb/usb.h"

typedef struct {
    uint8_t ls_x;
    uint8_t ls_y;
    uint8_t rs_x;
    uint8_t rs_y;

    union
    {
        struct
        {
            uint8_t dpad            : 4; // 0x08 is neutral
            bool button_square      : 1;
            bool button_cross       : 1;
            bool button_circle      : 1;
            bool button_triangle    : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            bool button_l1          : 1;
            bool button_r1          : 1;
            bool button_l2          : 1;
            bool button_r2          : 1;
            bool button_share       : 1;
            bool button_options     : 1;
            bool button_l3          : 1;
            bool button_r3          : 1;
        };
        uint8_t buttons_2;
    };

    union
    {
        struct
        {
            bool button_ps          : 1;
            bool button_touchpad    : 1;
            uint8_t button_reserved    : 6;
        };
        uint8_t buttons_3;
    };

    uint8_t trigger_l2;
    uint8_t trigger_r2;
    uint16_t imu_timestamp;
    uint8_t battery_lvl;

    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;

    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    // Leave the rest unpopulated

} __attribute__ ((packed)) ds4_input_s;

/** DS4 HID MODE **/
/**--------------------------**/
const tusb_desc_device_t ds4_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x054C,
    .idProduct = 0x09cc,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01
    };

const uint8_t ds4_hid_report_descriptor[] = {
        0x05, 0x01,         /*  Usage Page (Desktop),               */
        0x09, 0x05,         /*  Usage (Gamepad),                    */
        0xA1, 0x01,         /*  Collection (Application),           */
        0x85, 0x01,         /*      Report ID (1),                  */
        0x09, 0x30,         /*      Usage (X),                      */
        0x09, 0x31,         /*      Usage (Y),                      */
        0x09, 0x32,         /*      Usage (Z),                      */
        0x09, 0x35,         /*      Usage (Rz),                     */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
        0x75, 0x08,         /*      Report Size (8),                */
        0x95, 0x04,         /*      Report Count (4),               */
        0x81, 0x02,         /*      Input (Variable),               */
        0x09, 0x39,         /*      Usage (Hat Switch),             */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x25, 0x07,         /*      Logical Maximum (7),            */
        0x35, 0x00,         /*      Physical Minimum (0),           */
        0x46, 0x3B, 0x01,   /*      Physical Maximum (315),         */
        0x65, 0x14,         /*      Unit (Degrees),                 */
        0x75, 0x04,         /*      Report Size (4),                */
        0x95, 0x01,         /*      Report Count (1),               */
        0x81, 0x42,         /*      Input (Variable, Null State),   */
        0x65, 0x00,         /*      Unit,                           */
        0x05, 0x09,         /*      Usage Page (Button),            */
        0x19, 0x01,         /*      Usage Minimum (01h),            */
        0x29, 0x0E,         /*      Usage Maximum (0Eh),            */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x25, 0x01,         /*      Logical Maximum (1),            */
        0x75, 0x01,         /*      Report Size (1),                */
        0x95, 0x0E,         /*      Report Count (14),              */
        0x81, 0x02,         /*      Input (Variable),               */
        0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
        0x09, 0x20,         /*      Usage (20h),                    */
        0x75, 0x06,         /*      Report Size (6),                */
        0x95, 0x01,         /*      Report Count (1),               */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x25, 0x3F,         /*      Logical Maximum (63),           */
        0x81, 0x02,         /*      Input (Variable),               */
        0x05, 0x01,         /*      Usage Page (Desktop),           */
        0x09, 0x33,         /*      Usage (Rx),                     */
        0x09, 0x34,         /*      Usage (Ry),                     */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
        0x75, 0x08,         /*      Report Size (8),                */
        0x95, 0x02,         /*      Report Count (2),               */
        0x81, 0x02,         /*      Input (Variable),               */
        0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
        0x09, 0x21,         /*      Usage (21h),                    */
        0x95, 0x03,         /*      Report Count (3),               */
        0x81, 0x02,         /*      Input (Variable),               */
        0x05, 0x01,         /*      Usage Page (Desktop),           */
        0x19, 0x40,         /*      Usage Minimum (40h),            */
        0x29, 0x42,         /*      Usage Maximum (42h),            */
        0x16, 0x00, 0x80,   /*      Logical Minimum (-32768),       */
        0x26, 0x00, 0x7F,   /*      Logical Maximum (32767),        */
        0x75, 0x10,         /*      Report Size (16),               */
        0x95, 0x03,         /*      Report Count (3),               */
        0x81, 0x02,         /*      Input (Variable),               */
        0x19, 0x43,         /*      Usage Minimum (43h),            */
        0x29, 0x45,         /*      Usage Maximum (45h),            */
        0x16, 0x00, 0xE0,   /*      Logical Minimum (-8192),        */
        0x26, 0xFF, 0x1F,   /*      Logical Maximum (8191),         */
        0x95, 0x03,         /*      Report Count (3),               */
        0x81, 0x02,         /*      Input (Variable),               */
        0x06, 0x00, 0xFF,   /*      Usage Page (FF00h),             */
        0x09, 0x21,         /*      Usage (21h),                    */
        0x15, 0x00,         /*      Logical Minimum (0),            */
        0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
        0x75, 0x08,         /*      Report Size (8),                */
        0x95, 0x27,         /*      Report Count (39),              */
        0x81, 0x02,         /*      Input (Variable),               */
        0x85, 0x05,         /*      Report ID (5),                  */
        0x09, 0x22,         /*      Usage (22h),                    */
        0x95, 0x1F,         /*      Report Count (31),              */
        0x91, 0x02,         /*      Output (Variable),              */
        0x85, 0x04,         /*      Report ID (4),                  */
        0x09, 0x23,         /*      Usage (23h),                    */
        0x95, 0x24,         /*      Report Count (36),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x02,         /*      Report ID (2),                  */
        0x09, 0x24,         /*      Usage (24h),                    */
        0x95, 0x24,         /*      Report Count (36),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x08,         /*      Report ID (8),                  */
        0x09, 0x25,         /*      Usage (25h),                    */
        0x95, 0x03,         /*      Report Count (3),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x10,         /*      Report ID (16),                 */
        0x09, 0x26,         /*      Usage (26h),                    */
        0x95, 0x04,         /*      Report Count (4),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x11,         /*      Report ID (17),                 */
        0x09, 0x27,         /*      Usage (27h),                    */
        0x95, 0x02,         /*      Report Count (2),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x12,         /*      Report ID (18),                 */
        0x06, 0x02, 0xFF,   /*      Usage Page (FF02h),             */
        0x09, 0x21,         /*      Usage (21h),                    */
        0x95, 0x0F,         /*      Report Count (15),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x13,         /*      Report ID (19),                 */
        0x09, 0x22,         /*      Usage (22h),                    */
        0x95, 0x16,         /*      Report Count (22),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x14,         /*      Report ID (20),                 */
        0x06, 0x05, 0xFF,   /*      Usage Page (FF05h),             */
        0x09, 0x20,         /*      Usage (20h),                    */
        0x95, 0x10,         /*      Report Count (16),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x15,         /*      Report ID (21),                 */
        0x09, 0x21,         /*      Usage (21h),                    */
        0x95, 0x2C,         /*      Report Count (44),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x06, 0x80, 0xFF,   /*      Usage Page (FF80h),             */
        0x85, 0x80,         /*      Report ID (128),                */
        0x09, 0x20,         /*      Usage (20h),                    */
        0x95, 0x06,         /*      Report Count (6),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x81,         /*      Report ID (129),                */
        0x09, 0x21,         /*      Usage (21h),                    */
        0x95, 0x06,         /*      Report Count (6),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x82,         /*      Report ID (130),                */
        0x09, 0x22,         /*      Usage (22h),                    */
        0x95, 0x05,         /*      Report Count (5),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x83,         /*      Report ID (131),                */
        0x09, 0x23,         /*      Usage (23h),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x84,         /*      Report ID (132),                */
        0x09, 0x24,         /*      Usage (24h),                    */
        0x95, 0x04,         /*      Report Count (4),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x85,         /*      Report ID (133),                */
        0x09, 0x25,         /*      Usage (25h),                    */
        0x95, 0x06,         /*      Report Count (6),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x86,         /*      Report ID (134),                */
        0x09, 0x26,         /*      Usage (26h),                    */
        0x95, 0x06,         /*      Report Count (6),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x87,         /*      Report ID (135),                */
        0x09, 0x27,         /*      Usage (27h),                    */
        0x95, 0x23,         /*      Report Count (35),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x88,         /*      Report ID (136),                */
        0x09, 0x28,         /*      Usage (28h),                    */
        0x95, 0x22,         /*      Report Count (34),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x89,         /*      Report ID (137),                */
        0x09, 0x29,         /*      Usage (29h),                    */
        0x95, 0x02,         /*      Report Count (2),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x90,         /*      Report ID (144),                */
        0x09, 0x30,         /*      Usage (30h),                    */
        0x95, 0x05,         /*      Report Count (5),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x91,         /*      Report ID (145),                */
        0x09, 0x31,         /*      Usage (31h),                    */
        0x95, 0x03,         /*      Report Count (3),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x92,         /*      Report ID (146),                */
        0x09, 0x32,         /*      Usage (32h),                    */
        0x95, 0x03,         /*      Report Count (3),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0x93,         /*      Report ID (147),                */
        0x09, 0x33,         /*      Usage (33h),                    */
        0x95, 0x0C,         /*      Report Count (12),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA0,         /*      Report ID (160),                */
        0x09, 0x40,         /*      Usage (40h),                    */
        0x95, 0x06,         /*      Report Count (6),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA1,         /*      Report ID (161),                */
        0x09, 0x41,         /*      Usage (41h),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA2,         /*      Report ID (162),                */
        0x09, 0x42,         /*      Usage (42h),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA3,         /*      Report ID (163),                */
        0x09, 0x43,         /*      Usage (43h),                    */
        0x95, 0x30,         /*      Report Count (48),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA4,         /*      Report ID (164),                */
        0x09, 0x44,         /*      Usage (44h),                    */
        0x95, 0x0D,         /*      Report Count (13),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA5,         /*      Report ID (165),                */
        0x09, 0x45,         /*      Usage (45h),                    */
        0x95, 0x15,         /*      Report Count (21),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA6,         /*      Report ID (166),                */
        0x09, 0x46,         /*      Usage (46h),                    */
        0x95, 0x15,         /*      Report Count (21),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xF0,         /*      Report ID (240),                */
        0x09, 0x47,         /*      Usage (47h),                    */
        0x95, 0x3F,         /*      Report Count (63),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xF1,         /*      Report ID (241),                */
        0x09, 0x48,         /*      Usage (48h),                    */
        0x95, 0x3F,         /*      Report Count (63),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xF2,         /*      Report ID (242),                */
        0x09, 0x49,         /*      Usage (49h),                    */
        0x95, 0x0F,         /*      Report Count (15),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA7,         /*      Report ID (167),                */
        0x09, 0x4A,         /*      Usage (4Ah),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA8,         /*      Report ID (168),                */
        0x09, 0x4B,         /*      Usage (4Bh),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xA9,         /*      Report ID (169),                */
        0x09, 0x4C,         /*      Usage (4Ch),                    */
        0x95, 0x08,         /*      Report Count (8),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAA,         /*      Report ID (170),                */
        0x09, 0x4E,         /*      Usage (4Eh),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAB,         /*      Report ID (171),                */
        0x09, 0x4F,         /*      Usage (4Fh),                    */
        0x95, 0x39,         /*      Report Count (57),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAC,         /*      Report ID (172),                */
        0x09, 0x50,         /*      Usage (50h),                    */
        0x95, 0x39,         /*      Report Count (57),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAD,         /*      Report ID (173),                */
        0x09, 0x51,         /*      Usage (51h),                    */
        0x95, 0x0B,         /*      Report Count (11),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAE,         /*      Report ID (174),                */
        0x09, 0x52,         /*      Usage (52h),                    */
        0x95, 0x01,         /*      Report Count (1),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xAF,         /*      Report ID (175),                */
        0x09, 0x53,         /*      Usage (53h),                    */
        0x95, 0x02,         /*      Report Count (2),               */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0x85, 0xB0,         /*      Report ID (176),                */
        0x09, 0x54,         /*      Usage (54h),                    */
        0x95, 0x3F,         /*      Report Count (63),              */
        0xB1, 0x02,         /*      Feature (Variable),             */
        0xC0                /*  End Collection                      */
};

const uint8_t ds4_configuration_descriptor[] = {
        // Configuration number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

        // Interface
        9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
        // HID Descriptor
        9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(ds4_hid_report_descriptor)),
        // Endpoint Descriptor
        7, TUSB_DESC_ENDPOINT, 0x84, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4,
        // Endpoint Descriptor
        7, TUSB_DESC_ENDPOINT, 0x03, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4
    };

void ds4_hid_report(uint32_t timestamp)
{
    static ds4_input_s data = {.dpad = DI_HAT_CENTER};
    static uint8_t output_buffer[64] = {0};

    static analog_data_s analog_data = {0};
    static button_data_s button_data = {0};

    data.ls_x = (analog_data.lx>>4);
    data.ls_y = 255-(analog_data.ly>>4);
    data.rs_x = (analog_data.rx>>4);
    data.rs_y = 255-(analog_data.ry>>4);

    // Retrieve and write IMU data
    //imu_data_s *_imu_tmp = imu_fifo_last();
    // X input is tilting forward/backward
    // Z input is steering motion
    // Y input is tilting left/right

    // Tilt forward/Back
    //data.gyro_x = -_imu_tmp->gx;

    // Steer left/right
    //data.gyro_y = _imu_tmp->gz;

    // Tilt left/right. Invert input
    //data.gyro_z = _imu_tmp->gy;

    // Tilt forward/back
    //data.gyro_z = _imu_tmp->gx;

    //data.accel_x = _imu_tmp->ax;
    //data.accel_y = _imu_tmp->ay;
    //data.accel_z = _imu_tmp->az;

    data.imu_timestamp = 1;

    data.button_cross = button_data.button_b;
    data.button_circle = button_data.button_a;
    data.button_square = button_data.button_y;
    data.button_triangle = button_data.button_x;

    data.button_l3 = button_data.button_stick_left;
    data.button_r3 = button_data.button_stick_right;

    data.button_l1 = button_data.trigger_l;
    data.button_r1 = button_data.trigger_r;

    data.button_l2 = button_data.trigger_zl;
    data.button_r2 = button_data.trigger_zr;

    //data.trigger_l2 = button_data.zl_analog>>4;
    //data.trigger_r2 = button_data.zr_analog>>4;

    data.button_share = button_data.button_capture;
    data.button_options = button_data.button_plus;
    data.button_ps = button_data.button_home;
    data.button_touchpad = button_data.button_minus;

    uint8_t lr = 1 - button_data.dpad_left + button_data.dpad_right;
    uint8_t ud = 1 + button_data.dpad_up - button_data.dpad_down;

    data.dpad = dir_to_hat(HAT_MODE_DI, lr, ud);

    memcpy(output_buffer, &data, sizeof(ds4_input_s));
    tud_hid_report(0x01, output_buffer, 64);
}