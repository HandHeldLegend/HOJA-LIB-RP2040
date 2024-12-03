#ifndef INPUT_IMU_H
#define INPUT_IMU_H

#include "hoja_bsp.h"
#include "board_config.h"
#include <stdint.h>
#include <stdbool.h>

#include "settings_shared_types.h"

// IMU data structure
typedef struct
{
    union
    {
        struct
        {
            uint8_t ax_8l : 8;
            uint8_t ax_8h : 8;
        };
        int16_t ax;
    };

    union
    {
        struct
        {
            uint8_t ay_8l : 8;
            uint8_t ay_8h : 8;
        };
        int16_t ay;
    };
    
    union
    {
        struct
        {
            uint8_t az_8l : 8;
            uint8_t az_8h : 8;
        };
        int16_t az;
    };
    
    union
    {
        struct
        {
            uint8_t gx_8l : 8;
            uint8_t gx_8h : 8;
        };
        int16_t gx;
    };

    union
    {
        struct
        {
            uint8_t gy_8l : 8;
            uint8_t gy_8h : 8;
        };
        int16_t gy;
    };
    
    union
    {
        struct
        {
            uint8_t gz_8l : 8;
            uint8_t gz_8h : 8;
        };
        int16_t gz;
    };
    
    bool retrieved;
} imu_data_s;

typedef struct
{
    union
    {
        float raw[4];
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
    //uint32_t timestamp;
    int16_t ax;
    int16_t ay;
    int16_t az;
} quaternion_s;

#ifdef HOJA_IMU_CHAN_A_READ
    #define HOJA_IMU_DRIVER_ENABLED 1
#else
    #define HOJA_IMU_DRIVER_ENABLED 0
#endif

bool imu_init();

void imu_config_cmd(imu_cmd_t cmd, setting_callback_t cb);

bool imu_access_try(imu_data_s *out);
void imu_access_block(imu_data_s *out);

bool imu_quaternion_access_try(quaternion_s *out);
void imu_quaternion_access_block(quaternion_s *out);

void imu_task(uint32_t timestamp);


#endif
