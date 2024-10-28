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

typedef union 
{
    struct
    {
        bool     disabled : 1;
        uint16_t lower : 15;
        uint16_t upper : 16;
    };
    uint32_t value;
} trigger_calibration_u;

typedef struct
{
    // We use a settings version to
    // keep settings between updates
    uint16_t    settings_version;
} hoja_settings_version_parse_s;

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
    uint8_t rumble_mode;

    uint8_t switch_host_address[6]; // Host device we are paired to

    // Sub-Angle Notch Adjustments
    float l_sub_angles[8];
    float r_sub_angles[8];

    uint8_t rgb_mode;
    uint32_t rainbow_colors[6];
    uint8_t rgb_step_speed;

    uint16_t deadzone_left_center;
    uint16_t deadzone_left_outer;
    
    uint16_t deadzone_right_center;
    uint16_t deadzone_right_outer;
    
    uint16_t trigger_l_lower;
    uint16_t trigger_l_upper;
    uint16_t trigger_r_lower;
    uint16_t trigger_r_upper;

} hoja_settings_v1_s;

typedef struct
{
    // We use a settings version to
    // keep settings between updates
    uint16_t    settings_version;
    input_mode_t input_mode;
    uint8_t     switch_mac_address[6]; // Address of this controller (For USB)

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
    uint32_t rgb_colors[32];

    remap_profile_s remap_switch;
    remap_profile_s remap_xinput;
    remap_profile_s remap_gamecube;
    remap_profile_s remap_n64;
    remap_profile_s remap_snes;

    gc_sp_mode_t gc_sp_mode;
    uint8_t rumble_intensity;
    uint8_t gc_sp_light_trigger;
    uint8_t rumble_mode;

    uint8_t switch_host_address[6]; // Host device we are paired to (BEING DEPRECIATED)

    // Sub-Angle Notch Adjustments
    float l_sub_angles[8];
    float r_sub_angles[8];

    uint8_t rgb_mode;
    uint32_t rainbow_colors[6];
    uint8_t rgb_step_speed;

    uint16_t deadzone_left_center;
    uint16_t deadzone_left_outer;
    
    uint16_t deadzone_right_center;
    uint16_t deadzone_right_outer;
    
    trigger_calibration_u trigger_l;
    trigger_calibration_u trigger_r;

} hoja_settings_vcurrent_s;

typedef struct
{
    uint16_t magic; // Magic key (Match controller board ID)
    uint16_t charge_level; // Battery level (0-1000)
    uint16_t max_depletion_time; // Maximum time (minutes) to battery fully depleted
} hoja_settings_battery_storage_s;

extern hoja_settings_vcurrent_s global_loaded_settings;
extern hoja_settings_battery_storage_s global_loaded_battery_storage;

bool settings_get_bank();
bool settings_load();
void settings_set_charge_level(uint16_t charge_level);
void settings_set_max_depletion_time(uint16_t max_depletion_time);
void settings_save_battery_from_core0();
void settings_save_battery_from_core1();
void settings_core1_save_check();
void settings_save_webindicate();
void settings_save_from_core0();
void settings_reset_to_default();
void settings_set_centers(int lx, int ly, int rx, int ry);
void settings_set_distances(float *l_angle_distances, float *r_angle_distances);
void settings_set_angles(float *l_angles, float *r_angles);
void settings_set_mode(input_mode_t mode);
void settings_set_gc_sp(gc_sp_mode_t sp_mode);
void settings_set_snapback(uint8_t axis, uint8_t level);
void settings_set_rumble(uint8_t intensity, rumble_type_t type);
void settings_set_rumble_floor(uint8_t floor);
void settings_set_deadzone(uint8_t selection, uint16_t value);

#endif
