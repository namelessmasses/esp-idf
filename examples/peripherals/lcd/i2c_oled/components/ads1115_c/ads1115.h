#pragma once

#include "ads1115_i2c_codec.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle);

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg);

esp_err_t ads1115_write_register(i2c_master_dev_handle_t   ads1115_dev_handle,
                                 const ads1115_register_t *reg);

esp_err_t ads1115_get_single_conversion(i2c_master_dev_handle_t dev_handle,
                                        int16_t                *output);

esp_err_t ads1115_enable_conversion_ready_interrupt(
    i2c_master_dev_handle_t dev_handle,
    const uint8_t           ads1115_i2c_addr);
