#include "ads1115_i2c_codec.h"
#include "endian.h"

#include <esp_log.h>
#include <esp_log_level.h>
#include <limits.h>
#include <stdbool.h>

static const char *const TAG = "ads1115_i2c_codec";

void ads1115_encode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_host_reg,
    ads1115_address_pointer_register_t *const       out_i2c_reg) {

    out_i2c_reg->P = in_host_reg->P & 0b11;
}

void ads1115_decode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_i2c_reg,
    ads1115_address_pointer_register_t *const       out_host_reg) {

    out_host_reg->P = in_i2c_reg->P & 0b11;
}

#define ADS1115_ENCODING_REGISTER_BITS sizeof(uint16_t) * CHAR_BIT

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

#define ADS1115_ENCODING_CONFIG_FIELD(base_ptr_host, field)                    \
    (((base_ptr_host)->field & ADS1115_CONFIG_MASK_##field)                    \
     << ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field)

#define ADS1115_DECODING_CONFIG_FIELD(base_ptr_i2c, field)                     \
    (((base_ptr_i2c)->raw >>                                                   \
      ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field) &                      \
     ADS1115_CONFIG_MASK_##field)

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
}
