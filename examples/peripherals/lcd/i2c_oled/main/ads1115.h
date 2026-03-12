#pragma once

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include <stdint.h>
#include "esp_log_level.h"

#if !defined(ADS1115_SENSOR_ADDR)
#define ADS1115_SENSOR_ADDR 0x48 /*!< Address of the ADS1115 sensor */
#endif

typedef enum
{
    ADS1115_REG_CONVERSION = 0x00,
    ADS1115_REG_CONFIG     = 0x01,
    ADS1115_REG_LO_THRESH  = 0x02,
    ADS1115_REG_HI_THRESH  = 0x03
} ads1115_register_id_t;

typedef struct
{
    union
    {
        struct
        {
            uint8_t RESERVED : 6;
            uint8_t P : 2;
        };
        uint8_t val;
    };
} ads1115_address_pointer_register_t;

typedef struct
{
    uint16_t conversion_result;
} ads1115_conversion_register_t;

typedef enum
{
    ads1115_config_OS_WRITE_NO_EFFECT                 = 0,
    ads1115_config_OS_WRITE_START_SINGLE_CONVERSION   = 1,
    ads1115_config_OS_READ_CONVERSION_IN_PROGRESS     = 0,
    ads1115_config_OS_READ_CONVERSION_NOT_IN_PROGRESS = 1
} ads1115_config_os_t;

typedef enum
{
    ads1115_config_MUX_AIN0_AIN1 = 0,
    ads1115_config_MUX_AIN0_AIN3 = 1,
    ads1115_config_MUX_AIN1_AIN3 = 2,
    ads1115_config_MUX_AIN2_AIN3 = 3,
    ads1115_config_MUX_AIN0_GND  = 4,
    ads1115_config_MUX_AIN1_GND  = 5,
    ads1115_config_MUX_AIN2_GND  = 6,
    ads1115_config_MUX_AIN3_GND  = 7,
    ads1115_config_MUX_DEFAULT   = ads1115_config_MUX_AIN0_AIN1
} ads1115_config_mux_t;

typedef enum
{
    ads1115_config_PGA_6_144V    = 0,
    ads1115_config_PGA_4_096V    = 1,
    ads1115_config_PGA_2_048V    = 2,
    ads1115_config_PGA_1_024V    = 3,
    ads1115_config_PGA_0_512V    = 4,
    ads1115_config_PGA_0_256V    = 5,
    ads1115_config_PGA_RESERVED1 = 6,
    ads1115_config_PGA_RESERVED2 = 7,
    ads1115_config_PGA_DEFAULT   = ads1115_config_PGA_2_048V
} ads1115_config_pga_t;

typedef enum
{
    ads1115_config_MODE_CONTINUOUS  = 0,
    ads1115_config_MODE_SINGLE_SHOT = 1,
    ads1115_config_MODE_DEFAULT     = ads1115_config_MODE_SINGLE_SHOT
} ads1115_config_mode_t;

typedef enum
{
    ads1115_config_DR_8SPS    = 0,
    ads1115_config_DR_16SPS   = 1,
    ads1115_config_DR_32SPS   = 2,
    ads1115_config_DR_64SPS   = 3,
    ads1115_config_DR_128SPS  = 4,
    ads1115_config_DR_250SPS  = 5,
    ads1115_config_DR_475SPS  = 6,
    ads1115_config_DR_860SPS  = 7,
    ads1115_config_DR_DEFAULT = ads1115_config_DR_128SPS
} ads1115_config_dr_t;

typedef enum
{
    ads1115_config_COMP_MODE_TRADITIONAL = 0,
    ads1115_config_COMP_MODE_WINDOW      = 1,
    ads1115_config_COMP_MODE_DEFAULT     = ads1115_config_COMP_MODE_TRADITIONAL
} ads1115_config_comp_mode_t;

typedef enum
{
    ads1115_config_COMP_POL_ACTIVE_LOW  = 0,
    ads1115_config_COMP_POL_ACTIVE_HIGH = 1,
    ads1115_config_COMP_POL_DEFAULT     = ads1115_config_COMP_POL_ACTIVE_LOW
} ads1115_config_comp_pol_t;

typedef enum
{
    ads1115_config_COMP_LAT_NON_LATCHING = 0,
    ads1115_config_COMP_LAT_LATCHING     = 1,
    ads1115_config_COMP_LAT_DEFAULT      = ads1115_config_COMP_LAT_NON_LATCHING
} ads1115_config_comp_lat_t;

typedef enum
{
    ads1115_config_COMP_QUE_ASSERT_AFTER_ONE  = 0,
    ads1115_config_COMP_QUE_ASSERT_AFTER_TWO  = 1,
    ads1115_config_COMP_QUE_ASSERT_AFTER_FOUR = 2,
    ads1115_config_COMP_QUE_DISABLE           = 3,
    ads1115_config_COMP_QUE_DEFAULT           = ads1115_config_COMP_QUE_DISABLE
} ads1115_config_comp_que_t;

typedef struct
{
    union
    {
        struct
        {
            uint16_t OS : 1;
            uint16_t MUX : 3;
            uint16_t PGA : 3;
            uint16_t MODE : 1;
            uint16_t DR : 3;
            uint16_t COMP_MODE : 1;
            uint16_t COMP_POL : 1;
            uint16_t COMP_LAT : 1;
            uint16_t COMP_QUE : 2;
        };
        uint16_t val;
    };
} ads1115_config_register_t;

typedef struct
{
    uint16_t value;
} ads1115_lo_thresh_register_t;

typedef struct
{
    uint16_t value;
} ads1115_hi_thresh_register_t;

typedef struct
{
    ads1115_address_pointer_register_t address;
    union
    {
        ads1115_conversion_register_t conversion;
        ads1115_config_register_t     config;
        ads1115_lo_thresh_register_t  lo_thresh;
        ads1115_hi_thresh_register_t  hi_thresh;
    } reg;
} ads1115_register_t;

void ads1115_log_register(esp_log_level_t lvl, ads1115_register_t const *const reg);

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle);

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg);

esp_err_t ads1115_write_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                 const ads1115_register_t     *reg);
/**
 * @brief Enable the conversion ready interrupt for the ADS1115 device.
 *
 * @details Sets the most significant bit of HI_THRESH register to 0b1, the most
 * significant bit of LO_THRESH register to 0b0, and the COMP_QUE bits in the
 * CONFIG register to 0b00. This configures the ADS1115 to assert the ALERT/RDY
 * pin when a conversion is ready.
 *
 * @param dev_handle The device handle of the ADS1115.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t
ads1115_enable_conversion_ready_interrupt(i2c_master_dev_handle_t dev_handle,
                                          const uint8_t ads1115_i2c_addr);
