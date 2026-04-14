#pragma once

#include "sht41_reading.h"

#include <stdatomic.h>

typedef struct {
    struct {
        sht41_sensor_data_t temp_humid;
        float               voltage;
    } data[2];

    atomic_int_fast32_t index;

} sensor_data_t;

extern sensor_data_t g_sensor_data;
