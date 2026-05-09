/**
 * @file ns_lib_spi.c
 * @brief Emulated SPI flash reads for factory / pairing regions (NS-LIB-HID).
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo and
 * related marks are trademarks of their respective owners.
 */

#include "ns_lib_spi.h"
#include "ns_lib_analog.h"
#include "ns_lib_types.h"
#include "ns_lib_config.h"

#include <string.h>

/**
 * SPI factory stage-1/2 bytes from ns_devtype_t: 0x6012/0x6013 ID, 0x601B color flag, 0x605C SNES region.
 */
static void _ns_spi_factory_bytes_from_type(ns_devtype_t t, uint8_t *id_hi, uint8_t *id_lo, uint8_t *color_byte,
                                           uint8_t *snes_region_byte)
{
    *color_byte = 0x01;
    *snes_region_byte = 0x00;

    switch (t)
    {
    case NS_DEVTYPE_JOYCON_R:
        *id_hi = 0x01;
        *id_lo = 0x02;
        return;
    case NS_DEVTYPE_JOYCON_L:
        *id_hi = 0x02;
        *id_lo = 0x02;
        return;
    case NS_DEVTYPE_N64:
        *id_hi = 0x0C;
        *id_lo = 0x11;
        return;
    case NS_DEVTYPE_SNES_NA:
        *id_hi = 0x0B;
        *id_lo = 0x02;
        *color_byte = 0x02;
        *snes_region_byte = 0x00;
        return;
    case NS_DEVTYPE_SNES_JP:
        *id_hi = 0x0B;
        *id_lo = 0x02;
        *color_byte = 0x02;
        *snes_region_byte = 0x01;
        return;
    case NS_DEVTYPE_SNES_EU:
        *id_hi = 0x0B;
        *id_lo = 0x02;
        *color_byte = 0x02;
        *snes_region_byte = 0x02;
        return;
    case NS_DEVTYPE_FAMI:
        *id_hi = 0x07;
        *id_lo = 0x02;
        return;
    case NS_DEVTYPE_NES:
        *id_hi = 0x09;
        *id_lo = 0x02;
        return;
    case NS_DEVTYPE_MEGADRIVE:
    case NS_DEVTYPE_GENESIS:
        *id_hi = 0x0D;
        *id_lo = 0x02;
        return;
    case NS_DEVTYPE_PROCON:
    case NS_DEVTYPE_UNDEFINED:
    default:
        *id_hi = 0x03;
        *id_lo = 0x02;
        return;
    }
}


uint8_t _ns_spi_getaddressdata(uint8_t offset_address, uint8_t address)
{
    /*
     * SPI memory map notes (mirrors legacy switch_spi.c docs):
     *   0x00xx: patch ROM region (unused here)
     *   0x10xx: failsafe region (unused here)
     *   0x20xx..0x40xx: pairing block
     *   0x50xx: shipment info
     *   0x60xx: factory/config/calibration/color/sensor params
     *   0x80xx: user calibration region
     */
    ns_device_config_s cfg = {0};
    ns_device_config_get(&cfg);

    switch (offset_address)
    {
    case 0x00:
        /* Patch ROM not needed */
        return 0x00;

    case 0x10:
        /* Failsafe mechanism not needed */
        return 0x00;

    case 0x20 ... 0x40:
        /* Pairing data block */
        switch (address)
        {
        case 0x26:
        case 0x00:
            /* Pairing magic byte: 0x95 means pairing data present */
            return 0x95;

        case 0x27:
        case 0x01:
            /* Pairing payload size */
            return 0x22;

        case 0x28:
        case 0x29:
        case 0x02:
        case 0x03:
            /* Pairing checksum (not implemented) */
            return 0x00;

        case 0x2A ... 0x2F:
            /* Host BT address (big-endian mirror area) */
            return 0;
        case 0x04 ... 0x09:
            /* Host BT address mirror */
            return 0;

        case 0x30 ... 0x3F:
            /* Bluetooth LTK area (placeholder) */
            return 0x00;
        case 0x0A ... 0x19:
            /* Bluetooth LTK mirror */
            return 0x00;

        case 0x4A:
        case 0x24:
            /* Host capability: 0x68 Switch, 0x08 PC */
            return 0x68;

        case 0x4B:
        case 0x25:
            return 0;

        default:
            return 0x00;
        }

    case 0x50:
        /* Shipment flag: always 0 */
        return 0x00;

    case 0x60:
    {
        uint8_t *analog_cal = ns_analog_calibration_data();

        /* Factory configuration and calibration */
        uint8_t factory_id_hi;
        uint8_t factory_id_lo;
        uint8_t factory_color_byte;
        uint8_t factory_snes_region_byte;
        _ns_spi_factory_bytes_from_type(cfg.type, &factory_id_hi, &factory_id_lo, &factory_color_byte,
                                       &factory_snes_region_byte);

        switch (address)
        {
        case 0x00 ... 0x0F:
            /* Stage 1 (0x6000): serial number block, 0xFF means disabled */
            return 0xFF;

        // ProCon  0x03 0x02
        // N64     0x0C 0x11 
        // SNES    0x0B 0x02
        // Famicom 0x07 0x02
        // NES     0x09 0x02
        // Genesis 0x0D 0x02
        case 0x12:
            return factory_id_hi;

        case 0x13:
            return factory_id_lo;

        // Returns a bool indicating if color is set.
        // 0x02 if the controller is SNES
        case 0x1B:
            return factory_color_byte;

        case 0x20:
            return 35;

        case 0x21:
            return 0;

        case 0x22:
            return 185;

        case 0x23:
            return 255;

        case 0x24:
            return 26;

        case 0x25:
            return 1;

        case 0x26:
            return 0;

        case 0x27:
            return 64;

        case 0x28:
            return 0;

        case 0x29:
            return 64;

        case 0x2A:
            return 0;

        case 0x2B:
            return 64;

        case 0x2C:
            return 1;

        case 0x2D:
            return 0;

        case 0x2E:
            return 1;

        case 0x2F:
            return 0;

        case 0x30:
            return 1;

        case 0x31:
            return 0;

        case 0x32:
            return 0x3B;

        case 0x33:
            return 0x34;

        case 0x34:
            return 0x3B;

        case 0x35:
            return 0x34;

        case 0x36:
            return 0x3B;

        case 0x37:
            return 0x34;

        case 0x3D ... 0x45:
            /* Left stick factory calibration */
            return analog_cal[address - 0x3D];

        case 0x46 ... 0x4E:
            /* Right stick factory calibration */
            return analog_cal[address - 0x3D];

        case 0x4F:
            return 0xFF;

        case 0x50:
            /* Stage 2 (0x6050): body color (RGB) */
            return cfg.colors.body_r;
        case 0x51:
            return cfg.colors.body_g;
        case 0x52:
            return cfg.colors.body_b;

        case 0x53:
            /* Stage 2: button color (RGB) */
            return cfg.colors.buttons_r;

        case 0x54:
            return cfg.colors.buttons_g;

        case 0x55:
            return cfg.colors.buttons_b;

        case 0x56:
            /* Stage 2: left grip color (RGB) */
            return cfg.colors.l_grip_r;

        case 0x57:
            return cfg.colors.l_grip_g;

        case 0x58:
            return cfg.colors.l_grip_b;

        case 0x59:
            /* Stage 2: right grip color (RGB) */
            return cfg.colors.r_grip_r;

        case 0x5A:
            return cfg.colors.r_grip_g;

        case 0x5B:
            return cfg.colors.r_grip_b;

        // This is used for SNES controller color options
        // 0x00 - North America (Super Nintendo Purple)
        // 0x01 - Japan (Super Famicom)
        // 0x02 - Europe (Super Nintendo)
        case 0x5C:
            return factory_snes_region_byte;

        // Stage 3 configuration 0x6080, length 24
        // Covers factory sensor and stick device params
        // Start accelerometer offsets
        case 0x80:
            return 80;
        case 0x81:
            return 253;
        case 0x82:
            return 0;
        case 0x83:
            return 0;
        case 0x84:
            return 198;
        case 0x85:
            return 15;

        case 0x98:
        case 0x86:
            /* Stage 3/4: stick device parameters (mirrored) */
            return 15;

        case 0x99:
        case 0x87:
            return 48;

        case 0x9A:
        case 0x88:
            return 97;

        case 0x9B:
        case 0x89:
            return 174;

        case 0x9C:
        case 0x8A:
            return 144;

        case 0x9D:
        case 0x8B:
            return 217;

        case 0x9E:
        case 0x8C:
            return 212;

        case 0x9F:
        case 0x8D:
            return 20;

        case 0xA0:
        case 0x8E:
            return 84;

        case 0xA1:
        case 0x8F:
            return 65;

        case 0xA2:
        case 0x90:
            return 21;

        case 0xA3:
        case 0x91:
            return 84;

        case 0xA4:
        case 0x92:
            return 199;

        case 0xA5:
        case 0x93:
            return 121;

        case 0xA6:
        case 0x94:
            return 156;

        case 0xA7:
        case 0x95:
            return 51;

        case 0xA8:
        case 0x96:
            return 54;

        case 0xA9:
        case 0x97:
            return 99;

        default:
            return 0x00;
        }
    }

    case 0x80:
        /* User calibration block (unset => 0xFF) */
        switch (address)
        {
        case 0x10 ... 0x1A:
            /* User left-stick calibration magic */
            return 0xFF;
        case 0x1B ... 0x25:
            /* User right-stick calibration magic */
            return 0xFF;

        case 0x26 ... 0x3F:
            /* User gyro calibration area */
            return 0xFF;

        default:
            return 0xFF;
        }

    default:
        return 0xFF;
    }
}

void ns_spi_get(uint8_t offset_address, uint8_t address, uint8_t length, uint8_t *out)
{
    uint8_t read_info[5] = {address, offset_address, 0x00, 0x00, length};
    memcpy(&out[NS_SPI_STARTREAD_IDX], read_info, 5);

    for (int i = 0; i < length; i++)
    {
        out[NS_SPI_READ_OUTPUT_IDX + i] = _ns_spi_getaddressdata(offset_address, (uint8_t)(address + i));
    }
}
