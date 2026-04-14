#include <freertos/FreeRTOS.h>

#include "ads1115.h"
#include "endian.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_log_level.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const char *const TAG = "ads1115";

#define I2C_MASTER_FREQ_HZ 400000

void ads1115_encode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_host_reg,
    ads1115_address_pointer_register_t *const       out_i2c_reg) {

    // Bits [1:0] of the address pointer register specify the register address.
    // Bits [7:2] are reserved and should be set to 0.
    // Mask the input register's P field to ensure reserved bits are cleared,
    // and copy it to the output register.
    out_i2c_reg->P = in_host_reg->P & 0b11;
}

void ads1115_decode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_i2c_reg,
    ads1115_address_pointer_register_t *const       out_host_reg) {

    // Bits [1:0] of the address pointer register specify the register address.
    // Bits [7:2] are reserved and should be ignored.
    // Mask the input register's P field to extract the register address, and
    // copy it to the output register.
    out_host_reg->P = in_i2c_reg->P & 0b11;
}

#define ADS1115_ENCODING_REGISTER_BITS sizeof(uint16_t) * CHAR_BIT

// clang-format off
/**
* @defgroup ADS1115_I2C_ENCODING_CONFIG_FIELD_ENCODING_MACROS ADS1115 I2C Encoding Config Field Encoding Macros
* @brief Macros for encoding individual fields of the ADS1115 config register into
* their correct bit positions in the I2C wire format.
* @{
 */
// clang-format on

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_QUE 0

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_LAT                      \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_QUE +                       \
     ADS1115_CONFIG_BITS_COMP_QUE)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_POL                      \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_LAT +                       \
     ADS1115_CONFIG_BITS_COMP_LAT)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_MODE                     \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_POL +                       \
     ADS1115_CONFIG_BITS_COMP_POL)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_DR                            \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_COMP_MODE +                      \
     ADS1115_CONFIG_BITS_COMP_MODE)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_MODE                          \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_DR + ADS1115_CONFIG_BITS_DR)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_PGA                           \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_MODE + ADS1115_CONFIG_BITS_MODE)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_MUX                           \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_PGA + ADS1115_CONFIG_BITS_PGA)

#define ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_OS                            \
    (ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_MUX + ADS1115_CONFIG_BITS_MUX)

/**
 * @brief Encode a field of the ADS1115 config register into I2C wire format.
 *
 * @param base_ptr Pointer to the config register structure.
 * @param field The field to encode.
 * @return The encoded field value.
 */
#define ADS1115_ENCODING_CONFIG_FIELD(base_ptr_host, field)                    \
    (((base_ptr_host)->field & ADS1115_CONFIG_MASK_##field)                    \
     << ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field)

/**
 * @brief Decode a field of the ADS1115 config register from I2C wire format.
 *
 * @param base_ptr Pointer to the config register structure.
 * @param field The field to decode.
 * @return The decoded field value.
 */
#define ADS1115_DECODING_CONFIG_FIELD(base_ptr_i2c, field)                     \
    (((base_ptr_i2c)->raw >>                                                   \
      ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field) &                      \
     ADS1115_CONFIG_MASK_##field)

/** @} */ // End of ADS1115_I2C_ENCODING_CONFIG_FIELD_ENCODING_MACROS
void ads1115_encode_register(ads1115_register_t const *const in_host_reg,
                             ads1115_register_t *const       out_i2c_reg) {

    ads1115_encode_address_register_pointer(&in_host_reg->address,
                                            &out_i2c_reg->address);

    switch (in_host_reg->address.P & ADS1115_MASK_ADDRESS_POINTER_REGISTER) {
    case ADS1115_REG_CONVERSION:
    case ADS1115_REG_LO_THRESH:
    case ADS1115_REG_HI_THRESH:
        out_i2c_reg->raw = ads1115_endian_swap_16(in_host_reg->raw);
        break;

    case ADS1115_REG_CONFIG: {
        out_i2c_reg->raw =
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, OS);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, MUX);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, PGA);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, MODE);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, DR);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, COMP_MODE);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, COMP_POL);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, COMP_LAT);
        out_i2c_reg->raw |=
            ADS1115_ENCODING_CONFIG_FIELD(&in_host_reg->config, COMP_QUE);
        break;
    }

    default:
        ESP_LOGW(
            TAG,
            "Encoding register with unrecognized address pointer value: 0x%02x",
            in_host_reg->address.P);
        out_i2c_reg->raw = in_host_reg->raw;
        break;
    }
}

void ads1115_decode_register(ads1115_register_t const *const in_i2c_reg,
                             ads1115_register_t *const       out_host_reg) {

    ads1115_decode_address_register_pointer(&in_i2c_reg->address,
                                            &out_host_reg->address);
    switch (in_i2c_reg->address.P & ADS1115_MASK_ADDRESS_POINTER_REGISTER) {
    case ADS1115_REG_CONVERSION:
    case ADS1115_REG_LO_THRESH:
    case ADS1115_REG_HI_THRESH:
        out_host_reg->raw = ads1115_endian_swap_16(in_i2c_reg->raw);
        break;

    case ADS1115_REG_CONFIG: {
        out_host_reg->config.OS        = (in_i2c_reg->raw >> 15) & 0b1;
        out_host_reg->config.MUX       = (in_i2c_reg->raw >> 12) & 0b111;
        out_host_reg->config.PGA       = (in_i2c_reg->raw >> 9) & 0b111;
        out_host_reg->config.MODE      = (in_i2c_reg->raw >> 8) & 0b1;
        out_host_reg->config.DR        = (in_i2c_reg->raw >> 5) & 0b111;
        out_host_reg->config.COMP_MODE = (in_i2c_reg->raw >> 4) & 0b1;
        out_host_reg->config.COMP_POL  = (in_i2c_reg->raw >> 3) & 0b1;
        out_host_reg->config.COMP_LAT  = (in_i2c_reg->raw >> 2) & 0b1;
        out_host_reg->config.COMP_QUE  = in_i2c_reg->raw & 0b11;
        break;
    }

    default:
        out_host_reg->raw = in_i2c_reg->raw;
        break;
    }
}

float ads1115_gain_values[ads1115_config_PGA_SIZE] = {
    6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f};

void ads1115_log_register(esp_log_level_t                 lvl,
                          ads1115_register_t const *const reg) {
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "Register dump %p:", reg);
    if (reg == NULL) {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "<NULL register pointer>");
        return;
    }

    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "address.P = %02x", reg->address.P);
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "raw value = 0x%04x", reg->raw);
    switch (reg->address.P & ADS1115_MASK_ADDRESS_POINTER_REGISTER) {
    case ADS1115_REG_CONVERSION: {
        ESP_LOG_LEVEL_LOCAL(
            lvl, TAG, "reg_id............... : ADS1115_REG_CONVERSION");
        ESP_LOG_LEVEL_LOCAL(lvl,
                            TAG,
                            "  convesion_result... : 0x%04x (%d)",
                            (uint16_t)reg->conversion.conversion_result,
                            reg->conversion.conversion_result);
        break;
    }

    case ADS1115_REG_CONFIG: {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id........ : ADS1115_REG_CONFIG");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  OS.......... : %u", reg->config.OS);
        switch (reg->config.OS) {
        case ads1115_config_OS_WRITE_NO_EFFECT:
            ESP_LOG_LEVEL_LOCAL(
                lvl, TAG, "    WRITE:NO_EFFECT / READ:CONVERSION_IN_PROGRESS");
            break;
        case ads1115_config_OS_WRITE_START_SINGLE_CONVERSION:
            ESP_LOG_LEVEL_LOCAL(lvl,
                                TAG,
                                "    WRITE:START_SINGLE_CONVERSION / "
                                "READ:CONVERSION_NOT_IN_PROGRESS");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  MUX......... : %u", reg->config.MUX);
        switch (reg->config.MUX) {
        case ads1115_config_MUX_AIN0_AIN1:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN0 - AIN1");
            break;
        case ads1115_config_MUX_AIN0_AIN3:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN0 - AIN3");
            break;
        case ads1115_config_MUX_AIN1_AIN3:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN1 - AIN3");
            break;
        case ads1115_config_MUX_AIN2_AIN3:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN2 - AIN3");
            break;
        case ads1115_config_MUX_AIN0_GND:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN0 - GND");
            break;
        case ads1115_config_MUX_AIN1_GND:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN1 - GND");
            break;
        case ads1115_config_MUX_AIN2_GND:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN2 - GND");
            break;
        case ads1115_config_MUX_AIN3_GND:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    AIN3 - GND");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  PGA......... : %u", reg->config.PGA);
        switch (reg->config.PGA) {
        case ads1115_config_PGA_6_144V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 6.144V");
            break;
        case ads1115_config_PGA_4_096V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 4.096V");
            break;
        case ads1115_config_PGA_2_048V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 2.048V");
            break;
        case ads1115_config_PGA_1_024V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 1.024V");
            break;
        case ads1115_config_PGA_0_512V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 0.512V");
            break;
        case ads1115_config_PGA_0_256V:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 0.256V");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  MODE........ : %u", reg->config.MODE);
        switch (reg->config.MODE) {
        case ads1115_config_MODE_CONTINUOUS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Continuous conversion mode");
            break;
        case ads1115_config_MODE_SINGLE_SHOT:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Single-shot conversion mode");
            break;
        default:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  DR.......... : %u", reg->config.DR);
        switch (reg->config.DR) {
        case ads1115_config_DR_8SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "      8 sps");
            break;
        case ads1115_config_DR_16SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "      16 sps");
            break;
        case ads1115_config_DR_32SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "     32 sps");
            break;
        case ads1115_config_DR_64SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "     64 sps");
            break;
        case ads1115_config_DR_128SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    128 sps");
            break;
        case ads1115_config_DR_250SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    250 sps");
            break;
        case ads1115_config_DR_475SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    475 sps");
            break;
        case ads1115_config_DR_860SPS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    860 sps");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(
            lvl, TAG, "  COMP_MODE... : %u", reg->config.COMP_MODE);
        switch (reg->config.COMP_MODE) {
        case ads1115_config_COMP_MODE_TRADITIONAL:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Traditional");
            break;
        case ads1115_config_COMP_MODE_WINDOW:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Window");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(
            lvl, TAG, "  COMP_POL.... : %u", reg->config.COMP_POL);
        switch (reg->config.COMP_POL) {
        case ads1115_config_COMP_POL_ACTIVE_LOW:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Active LOW");
            break;
        case ads1115_config_COMP_POL_ACTIVE_HIGH:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Active HIGH");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(
            lvl, TAG, "  COMP_LAT.... : %u", reg->config.COMP_LAT);
        switch (reg->config.COMP_LAT) {
        case ads1115_config_COMP_LAT_NON_LATCHING:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Non-Latching");
            break;
        case ads1115_config_COMP_LAT_LATCHING:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Latching");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(
            lvl, TAG, "  COMP_QUE.... : %u", reg->config.COMP_QUE);
        switch (reg->config.COMP_QUE) {
        case ads1115_config_COMP_QUE_ASSERT_AFTER_ONE:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 1");
            break;
        case ads1115_config_COMP_QUE_ASSERT_AFTER_TWO:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 2");
            break;
        case ads1115_config_COMP_QUE_ASSERT_AFTER_FOUR:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 4");
            break;
        case ads1115_config_COMP_QUE_DISABLE:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Disabled");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }
        break;
    }

    case ADS1115_REG_LO_THRESH: {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id.... : ADS1115_REG_LO_THRESH");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  value... : %hu", reg->lo_thresh.value);
        break;
    }

    case ADS1115_REG_HI_THRESH: {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id.... : ADS1115_REG_HI_THRESH");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  value... : %hu", reg->hi_thresh.value);
        break;
    }

    default:
        ESP_LOGW(TAG, "Unknown register id");
    }
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

    float val = FS * conversion->conversion_result / 32768.f;
    ESP_LOGD(
        TAG, "%f * %d / 32768 = %f", FS, conversion->conversion_result, val);
    return val;
}

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle) {
    i2c_device_config_t ads1115_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ads1115_i2c_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ};

    esp_err_t ret =
        i2c_master_bus_add_device(bus, &ads1115_dev_config, ads1115_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to add ADS1115 device to I2C bus: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg) {
    if (reg == NULL) {
        ESP_LOGE(TAG, "Cannot read register: NULL pointer provided");
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

    ESP_LOGD(TAG, "Received (decoded): 0x%04x", reg->raw);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register: %s", esp_err_to_name(ret));
        ads1115_log_register(ESP_LOG_ERROR, reg);
    }

    return ret;
}

esp_err_t ads1115_write_register(i2c_master_dev_handle_t   ads1115_dev_handle,
                                 const ads1115_register_t *reg) {
    if (reg == NULL) {
        ESP_LOGE(TAG, "Cannot write register: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    ads1115_register_t reg_encoded;
#if ADS1115_DRIVER_CONVERTS_ENDIANESS
    ads1115_register_encode(reg, &reg_encoded);
#else
    reg_encoded = *reg;
#endif // ADS1115_DRIVER_CONVERTS_ENDIANESS
    ESP_LOGD(TAG, "Writing register (host)   : 0x%04x", reg->raw);
    ESP_LOGD(TAG, "Writing register (encoded): 0x%04x", reg_encoded.raw);

    esp_err_t ret = i2c_master_transmit(ads1115_dev_handle,
                                        (uint8_t const *)&reg_encoded,
                                        sizeof(ads1115_register_t),
                                        100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register: %s", esp_err_to_name(ret));
        ads1115_log_register(ESP_LOG_ERROR, reg);
    }

    return ret;
}

esp_err_t ads1115_get_single_conversion(i2c_master_dev_handle_t dev_handle,
                                        int16_t                *output) {
    if (output == NULL) {
        ESP_LOGE(TAG, "Cannot get single conversion: NULL output pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // Read the current config register to preserve settings like PGA, MUX, etc.
    ads1115_register_t reg = {0};
    reg.address.P          = ADS1115_REG_CONFIG;
    esp_err_t ret          = ads1115_read_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG, "Failed to read config register: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set the OS bit to start a single conversion
    reg.config.OS = ads1115_config_OS_WRITE_START_SINGLE_CONVERSION;

    ret = ads1115_write_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG, "Failed to start single conversion: %s", esp_err_to_name(ret));
        return ret;
    }

    bool isBusy = true;
    do {
        // Wait for conversion to complete
        vTaskDelay(pdMS_TO_TICKS(100));

        reg.raw = 0;
        ret     = ads1115_read_register(dev_handle, &reg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG,
                     "Failed to read config register during conversion: %s",
                     esp_err_to_name(ret));
            return ret;
        }

        isBusy =
            (reg.config.OS == ads1115_config_OS_READ_CONVERSION_IN_PROGRESS);
    } while (isBusy);

    reg.address.P = ADS1115_REG_CONVERSION;
    reg.raw       = 0;
    ret           = ads1115_read_register(dev_handle, &reg);
    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG, "Failed to read conversion result: %s", esp_err_to_name(ret));
        return ret;
    }

    *output = reg.conversion.conversion_result;
    return ESP_OK;
}

esp_err_t
ads1115_enable_conversion_ready_interrupt(i2c_master_dev_handle_t dev_handle,
                                          const uint8_t ads1115_i2c_addr) {
    return ESP_ERR_NOT_SUPPORTED;
}
