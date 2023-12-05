#ifndef HOJA_SETTINGS_H
#define HOJA_SETTINGS_H

#include "hoja_includes.h"

typedef struct
{
    button_remap_s  remap;
    buttons_unset_s disabled;
} remap_profile_s;

typedef union
{
    struct
    {
        int center : 31;
        bool invert : 1;
    };
    int value;
    
} analog_center_u;

typedef struct
{
    // We use a settings version to
    // keep settings between updates
    uint16_t    settings_version;
    input_mode_t input_mode;
    uint8_t     switch_mac_address[6]; // Address of this controller

    analog_center_u lx_center;
    analog_center_u ly_center;

    analog_center_u rx_center;
    analog_center_u ry_center;

    // Angles of most distant cardinal directions
    float l_angles[8];
    float r_angles[8];

    // Angle distances one for each of the 8 directions
    // This is just stored as the literal raw angle value
    // ANGLE ORDER
    // E, NE, N, NW, W, SW, S, SE
    float l_angle_distances[8];
    float r_angle_distances[8];

    // IMU Offsets for more precision
    int8_t imu_0_offsets[6];
    int8_t imu_1_offsets[6];

    // RGB Color Store
    uint32_t rgb_colors[12];

    remap_profile_s remap_switch;
    remap_profile_s remap_xinput;
    remap_profile_s remap_gamecube;
    remap_profile_s remap_n64;
    remap_profile_s remap_snes;

    gc_sp_mode_t gc_sp_mode;
    uint8_t rumble_intensity;
    uint8_t gc_sp_light_trigger;
    uint8_t rumble_floor;

    uint8_t switch_host_address[6]; // Host device we are paired to

    // Sub-Angle Notch Adjustments
    float l_sub_angles[8];
    float r_sub_angles[8];

} hoja_settings_s;

extern hoja_settings_s global_loaded_settings;

bool settings_load();
void settings_core1_save_check();
void settings_save_webindicate();
void settings_save();
void settings_reset_to_default();
void settings_set_centers(int lx, int ly, int rx, int ry);
void settings_set_distances(float *l_angle_distances, float *r_angle_distances);
void settings_set_angles(float *l_angles, float *r_angles);
void settings_set_mode(input_mode_t mode);
void settings_set_gc_sp(gc_sp_mode_t sp_mode);
void settings_set_snapback(uint8_t axis, uint8_t level);
void settings_set_rumble(uint8_t intensity);
void settings_set_rumble_floor(uint8_t floor);

#endif
