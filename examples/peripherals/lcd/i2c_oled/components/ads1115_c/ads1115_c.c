#include <freertos/FreeRTOS.h>

#include "ads1115.h"

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <freertos/task.h>
#include <math.h>
#include <stdbool.h>


#define I2C_MASTER_FREQ_HZ 400000

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle) {
    i2c_device_config_t ads1115_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ads1115_i2c_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ};

    return i2c_master_bus_add_device(
        bus, &ads1115_dev_config, ads1115_dev_handle);
}

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg) {
    if (reg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ads1115_register_t reg_encoded;
    ads1115_encode_register(reg, &reg_encoded);

    esp_err_t ret =
        i2c_master_transmit_receive(ads1115_dev_handle,
                                    (uint8_t const *)&reg_encoded.address,
                                    sizeof(ads1115_address_pointer_register_t),
                                    (uint8_t *)&reg_encoded,
                                    sizeof(ads1115_register_t),
                                    100);

#if ADS1115_DRIVER_CONVERTS_ENDIANESS
    ads1115_register_decode(&reg_encoded, reg);
#else
    *reg = reg_encoded;
#endif

    return ret;
}

esp_err_t ads1115_write_register(i2c_master_dev_handle_t   ads1115_dev_handle,
                                 const ads1115_register_t *reg) {
    if (reg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ads1115_register_t reg_encoded;
#if ADS1115_DRIVER_CONVERTS_ENDIANESS
    ads1115_register_encode(reg, &reg_encoded);
#else
    reg_encoded = *reg;
#endif

    return i2c_master_transmit(ads1115_dev_handle,
                               (uint8_t const *)&reg_encoded,
                               sizeof(ads1115_register_t),
                               100);
}

esp_err_t ads1115_get_single_conversion(i2c_master_dev_handle_t dev_handle,
                                        int16_t                *output) {
    if (output == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ads1115_register_t reg = {0};
    reg.address.P          = ADS1115_REG_CONFIG;
    esp_err_t ret          = ads1115_read_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        return ret;
    }

    reg.config.OS = ads1115_config_OS_WRITE_START_SINGLE_CONVERSION;

    ret = ads1115_write_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        return ret;
    }

    bool isBusy = true;
    do {
        vTaskDelay(pdMS_TO_TICKS(100));

        reg.raw = 0;
        ret     = ads1115_read_register(dev_handle, &reg);
        if (ret != ESP_OK) {
            return ret;
        }

        isBusy =
            (reg.config.OS == ads1115_config_OS_READ_CONVERSION_IN_PROGRESS);
    } while (isBusy);

    reg.address.P = ADS1115_REG_CONVERSION;
    reg.raw       = 0;
    ret           = ads1115_read_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        return ret;
    }

    *output = reg.conversion.conversion_result;
    return ESP_OK;
}

float ads1115_get_voltage(
    ads1115_config_pga_t                       pga,
    ads1115_conversion_register_t const *const conversion) {
    float FS = 0.f;
    switch (pga) {
    case ads1115_config_PGA_6_144V:
        FS = 6.144f;
        break;
    case ads1115_config_PGA_4_096V:
        FS = 4.096f;
        break;
    case ads1115_config_PGA_2_048V:
        FS = 2.048f;
        break;
    case ads1115_config_PGA_1_024V:
        FS = 1.024f;
        break;
    case ads1115_config_PGA_0_512V:
        FS = 0.512f;
        break;
    case ads1115_config_PGA_0_256V:
        FS = 0.256f;
        break;
    default:
        return NAN;
    }

    return FS * conversion->conversion_result / 32768.f;
}

esp_err_t
ads1115_enable_conversion_ready_interrupt(i2c_master_dev_handle_t dev_handle,
                                          const uint8_t ads1115_i2c_addr) {
    (void)dev_handle;
    (void)ads1115_i2c_addr;
    return ESP_ERR_NOT_SUPPORTED;
}
