#ifndef HOJA_IMU_H
#define HOJA_IMU_H

#include <stdint.h>
#include "drivers/drivers.h"

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
} __attribute__ ((packed)) imu_data_s;

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

typedef enum
{
    IMU_ACCESS_RAW_DATA, 
    IMU_ACCESS_QUATERNION_DATA,
} imu_access_t;

#ifdef HOJA_IMU_CHAN_A_READ
    #define HOJA_IMU_DRIVER_ENABLED 1
#else
    #define HOJA_IMU_DRIVER_ENABLED 0
#endif

void imu_access(imu_data_s *out, imu_access_t type);

void imu_task(uint32_t timestamp);

void imu_pack_quat(mode_2_s *out);
void imu_get_quat(quaternion_s *out);

imu_data_s* imu_fifo_last();
imu_data_s* imu_fifo_pop();
void imu_fifo_push(imu_data_s *imu_data, uint32_t timestamp);
void imu_calibrate_start();
void imu_calibrate_stop();
void imu_register(imu_data_s *data_a, imu_data_s *data_b);
void imu_set_enabled(bool enable);

#endif
