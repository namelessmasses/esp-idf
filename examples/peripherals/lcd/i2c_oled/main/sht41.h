#pragma once

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include <stdint.h>

#if !defined(SHT41_SENSOR_ADDR)
#define SHT41_SENSOR_ADDR 0x44 /*!< Address of the SHT41 sensor */
#endif

esp_err_t sht41_bus_add_device(i2c_master_bus_handle_t  bus,
                               const uint8_t            sht41_i2c_addr,
                               i2c_master_dev_handle_t *sht41_dev_handle);

typedef enum
{
    CMD_READ_HIGH_PRECISION   = 0xFD,
    CMD_READ_MEDIUM_PRECISION = 0xF6,
    CMD_READ_LOW_PRECISION    = 0xE0,
    CMD_READ_SERIAL_NUMBER    = 0x89,
    CMD_HEATED_200mW_1000ms   = 0x39,
    CMD_HEATED_200mW_100ms    = 0x32,
    CMD_HEATED_110mW_1000ms   = 0x2F,
    CMD_HEATED_110mW_100ms    = 0x24,
    CMD_HEATED_20mW_1000ms    = 0x1E,
    CMD_HEATED_20mW_100ms     = 0x15
} sht41_command_t;

const char *sht41_get_cmd_string(sht41_command_t cmd);

typedef struct
{
    uint16_t temp_ticks;
    float    temperature_celcius;
    float    temperature_fahrenheit;
    uint16_t rh_ticks;
    float    relative_humidity;
} sht41_data_t;

void sht41_print_data(const sht41_data_t *data);

typedef struct
{
    enum
    {
        POWER_20mW,
        POWER_110mW,
        POWER_200mW
    } power;

    enum
    {
        SHORT = 100,
        LONG  = 1000
    } duration;
} heating_config_t;

esp_err_t sht41_get_reading(i2c_master_dev_handle_t dev_handle,
                            sht41_command_t command, sht41_data_t *data,
                            uint32_t wait_duration_ms);

esp_err_t
sht41_get_reading_with_heating(i2c_master_dev_handle_t dev_handle,
                               sht41_data_t           *data,
                               const heating_config_t *heating_config);
