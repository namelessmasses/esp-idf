#include "ads1115_i2c_codec.h"
#include "unity.h"

TEST_CASE("[ads1115]..address_register_pointer.encode",
          "ads1115 encode address register pointer") {

    ads1115_address_pointer_register_t host_reg;
    ads1115_address_pointer_register_t i2c_reg;

    host_reg.P = ADS1115_REG_CONVERSION;
    ads1115_encode_address_register_pointer(&host_reg, &i2c_reg);

    // Bits[7:2] should be 0, bits[1:0] should be the register address.
    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION, i2c_reg.P);
}

TEST_CASE("[ads1115]..address_register_pointer.decode",
          "ads1115 decode address register pointer") {

    ads1115_address_pointer_register_t i2c_reg;
    ads1115_address_pointer_register_t host_reg;

    i2c_reg.P = ADS1115_REG_CONFIG;
    ads1115_decode_address_register_pointer(&i2c_reg, &host_reg);

    // Bits[7:2] should be ignored, bits[1:0] should be the register address.
    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONFIG, host_reg.P);
}

TEST_CASE("[ads1115]..config_register.encode.default",
          "default host config register (0x8583) encodes to i2c 0x8385") {
    ads1115_register_t config_reg_host{
        .address = {.P = ADS1115_REG_CONFIG},
        .config  = {.OS        = ads1115_config_OS_DEFAULT,
                    .MUX       = ads1115_config_MUX_DEFAULT,
                    .PGA       = ads1115_config_PGA_DEFAULT,
                    .MODE      = ads1115_config_MODE_DEFAULT,
                    .DR        = ads1115_config_DR_DEFAULT,
                    .COMP_MODE = ads1115_config_COMP_MODE_DEFAULT,
                    .COMP_POL  = ads1115_config_COMP_POL_DEFAULT,
                    .COMP_LAT  = ads1115_config_COMP_LAT_DEFAULT,
                    .COMP_QUE  = ads1115_config_COMP_QUE_DEFAULT}};

    ads1115_register_t config_reg_i2c;
    ads1115_encode_register(&config_reg_host, &config_reg_i2c);
    TEST_ASSERT_EQUAL_UINT16(0x8385, config_reg_i2c.raw);
}

TEST_CASE("[ads1115]..config_register.decode.default",
          "default i2c config register 0x8583, decodes to expected default "
          "config register") {
    ads1115_register_t config_reg_i2c = {.address = {.P = ADS1115_REG_CONFIG},
                                         .raw     = 0x8385};

    ads1115_register_t config_reg_host;

    ads1115_decode_register(&config_reg_i2c, &config_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONFIG, config_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_OS_DEFAULT,
                            config_reg_host.config.OS);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_MUX_DEFAULT,
                            config_reg_host.config.MUX);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_PGA_DEFAULT,
                            config_reg_host.config.PGA);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_MODE_DEFAULT,
                            config_reg_host.config.MODE);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_DR_DEFAULT,
                            config_reg_host.config.DR);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_COMP_MODE_DEFAULT,
                            config_reg_host.config.COMP_MODE);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_COMP_POL_DEFAULT,
                            config_reg_host.config.COMP_POL);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_COMP_LAT_DEFAULT,
                            config_reg_host.config.COMP_LAT);
    TEST_ASSERT_EQUAL_UINT8(ads1115_config_COMP_QUE_DEFAULT,
                            config_reg_host.config.COMP_QUE);
}

TEST_CASE("[ads1115]..conversion_register.decode_0",
          "i2c conversion 0x0000 decodes to host conversion with value 0") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0x0000};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(0x0000,
                             conversion_reg_host.conversion.conversion_result);
}

TEST_CASE("[ads1115]..conversion_register.decode_1",
          "i2c big-endian conversion 0x0100 decodes to esp-32 host conversion "
          "with value 1") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0x0100};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(0x0001,
                             conversion_reg_host.conversion.conversion_result);
}

TEST_CASE("[ads1115]..conversion_register.decode_negative_1",
          "i2c big-endian negative conversion 0xFFFF decodes to esp-32 host "
          "conversion with value -1") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0xFFFF};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF,
                             conversion_reg_host.conversion.conversion_result);
}

TEST_CASE("[ads1115]..conversion_register.decode_negative_256",
          "i2c big-endian negative conversion 0xFF00 decodes to esp-32 host "
          "conversion with value -256") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0x00FF};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(static_cast<int16_t>(0xFF00),
                             conversion_reg_host.conversion.conversion_result);
}

TEST_CASE("[ads1115]..conversion_register.decode_negative_32768",
          "i2c big-endian negative conversion 0x8000 decodes to esp-32 host "
          "conversion with value -32768") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0x0080};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(0x8000,
                             conversion_reg_host.conversion.conversion_result);
}

TEST_CASE("[ads1115]..conversion_register.decode_32767",
          "i2c big-endian conversion 0x7FFF decodes to esp-32 host conversion "
          "with value 32767") {
    ads1115_register_t conversion_reg_i2c = {
        .address = {.P = ADS1115_REG_CONVERSION}, .raw = 0xFF7F};

    ads1115_register_t conversion_reg_host;
    ads1115_decode_register(&conversion_reg_i2c, &conversion_reg_host);

    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION,
                            conversion_reg_host.address.P);
    TEST_ASSERT_EQUAL_UINT16(0x7FFF,
                             conversion_reg_host.conversion.conversion_result);
}
