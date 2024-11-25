#ifndef SWITCH_MOTION_H
#define SWITCH_MOTION_H

#include <stdint.h>
#include <stddef.h>
#include "input/imu.h"

typedef struct{
    int16_t y;
    int16_t x;
    int16_t z;
} vector_s;

typedef struct {
    vector_s accel_0;
    uint32_t mode : 2;
    uint32_t max_index : 2;
    uint32_t last_sample_0 : 21;
    uint32_t last_sample_1l : 7;
    uint16_t last_sample_1h : 14;
    uint16_t last_sample_2l : 2;
    vector_s accel_1;
    uint32_t last_sample_2h : 19;
    uint32_t delta_last_first_0 : 13;
    uint16_t delta_last_first_1 : 13;
    uint16_t delta_last_first_2l : 3;
    vector_s accel_2;
    uint32_t delta_last_first_2h : 10;
    uint32_t delta_mid_avg_0 : 7;
    uint32_t delta_mid_avg_1 : 7;
    uint32_t delta_mid_avg_2 : 7;
    uint32_t timestamp_start_l : 1;
    uint16_t timestamp_start_h : 10;
    uint16_t timestamp_count : 6;
} mode_2_s;

void switch_motion_pack_quat(quaternion_s *in, mode_2_s *out);

#endif