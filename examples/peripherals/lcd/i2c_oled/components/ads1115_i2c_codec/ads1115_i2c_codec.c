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

#define ADS1115_ENCODING_CONFIG_FIELD(base_ptr_host, base_ptr_i2c, field)      \
    ((base_ptr_i2c)->raw |=                                                     \
     (((base_ptr_host)->config.field & ADS1115_CONFIG_MASK_##field)             \
      << ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field))

#define ADS1115_DECODING_CONFIG_FIELD(base_ptr_i2c, base_ptr_host, field)      \
    ((base_ptr_host)->config.field =                                              \
         (((base_ptr_i2c)->raw >>                                                 \
           ADS1115_ENCODING_CONFIG_I2C_BIT_POSITION_##field) &                 \
          ADS1115_CONFIG_MASK_##field))

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
        out_i2c_reg->raw = 0;
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, OS);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, MUX);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, PGA);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, MODE);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, DR);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, COMP_MODE);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, COMP_POL);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, COMP_LAT);
        ADS1115_ENCODING_CONFIG_FIELD(in_host_reg, out_i2c_reg, COMP_QUE);
        out_i2c_reg->raw = ads1115_endian_swap_16(out_i2c_reg->raw);
        break;
    }

    default:
        // clang-format off
        ESP_LOGW(TAG,
            "Encoding register with unrecognized address pointer value: 0x%02x",
            in_host_reg->address.P);
        // clang-format on
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
        ads1115_register_t raw_after_endian = {
            .raw = ads1115_endian_swap_16(in_i2c_reg->raw)};
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, OS);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, MUX);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, PGA);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, MODE);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, DR);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, COMP_MODE);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, COMP_POL);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, COMP_LAT);
        ADS1115_DECODING_CONFIG_FIELD(&raw_after_endian, out_host_reg, COMP_QUE);
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
