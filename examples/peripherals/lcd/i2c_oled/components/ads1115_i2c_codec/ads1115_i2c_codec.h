#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_log_level.h>
#include <stdint.h>

#if !defined(ADS1115_SENSOR_ADDR)
#define ADS1115_SENSOR_ADDR 0x48
#endif

#pragma pack(push, 1)
typedef enum {
    ADS1115_REG_CONVERSION = 0b00,
    ADS1115_REG_CONFIG     = 0b01,
    ADS1115_REG_LO_THRESH  = 0b10,
    ADS1115_REG_HI_THRESH  = 0b11
} ads1115_register_id_t;

#define ADS1115_MASK_ADDRESS_POINTER_REGISTER 0b11

typedef struct {
    uint8_t P;
} ads1115_address_pointer_register_t;

static_assert(sizeof(ads1115_address_pointer_register_t) == 1,
              "Address pointer register must be 1 byte");

typedef struct {
    int16_t conversion_result;
} ads1115_conversion_register_t;

typedef enum {
    ads1115_config_OS_WRITE_NO_EFFECT                 = 0,
    ads1115_config_OS_WRITE_START_SINGLE_CONVERSION   = 1,
    ads1115_config_OS_READ_CONVERSION_IN_PROGRESS     = 0,
    ads1115_config_OS_READ_CONVERSION_NOT_IN_PROGRESS = 1,
    ads1115_config_OS_DEFAULT = ads1115_config_OS_WRITE_START_SINGLE_CONVERSION
} ads1115_config_os_t;

typedef enum {
    ads1115_config_MUX_AIN0_AIN1 = 0b000,
    ads1115_config_MUX_AIN0_AIN3 = 0b001,
    ads1115_config_MUX_AIN1_AIN3 = 0b010,
    ads1115_config_MUX_AIN2_AIN3 = 0b011,
    ads1115_config_MUX_AIN0_GND  = 0b100,
    ads1115_config_MUX_AIN1_GND  = 0b101,
    ads1115_config_MUX_AIN2_GND  = 0b110,
    ads1115_config_MUX_AIN3_GND  = 0b111,
    ads1115_config_MUX_DEFAULT   = ads1115_config_MUX_AIN0_AIN1
} ads1115_config_mux_t;

typedef enum {
    ads1115_config_PGA_6_144V    = 0b000,
    ads1115_config_PGA_4_096V    = 0b001,
    ads1115_config_PGA_2_048V    = 0b010,
    ads1115_config_PGA_1_024V    = 0b011,
    ads1115_config_PGA_0_512V    = 0b100,
    ads1115_config_PGA_0_256V    = 0b101,
    ads1115_config_PGA_SIZE      = 0b110,
    ads1115_config_PGA_RESERVED1 = ads1115_config_PGA_0_256V,
    ads1115_config_PGA_RESERVED2 = ads1115_config_PGA_0_256V,
    ads1115_config_PGA_DEFAULT   = ads1115_config_PGA_2_048V
} ads1115_config_pga_t;

extern float ads1115_gain_values[ads1115_config_PGA_SIZE];

typedef enum {
    ads1115_config_MODE_CONTINUOUS  = 0,
    ads1115_config_MODE_SINGLE_SHOT = 1,
    ads1115_config_MODE_DEFAULT     = ads1115_config_MODE_SINGLE_SHOT
} ads1115_config_mode_t;

typedef enum {
    ads1115_config_DR_8SPS    = 0b000,
    ads1115_config_DR_16SPS   = 0b001,
    ads1115_config_DR_32SPS   = 0b010,
    ads1115_config_DR_64SPS   = 0b011,
    ads1115_config_DR_128SPS  = 0b100,
    ads1115_config_DR_250SPS  = 0b101,
    ads1115_config_DR_475SPS  = 0b110,
    ads1115_config_DR_860SPS  = 0b111,
    ads1115_config_DR_DEFAULT = ads1115_config_DR_128SPS
} ads1115_config_dr_t;

typedef enum {
    ads1115_config_COMP_MODE_TRADITIONAL = 0,
    ads1115_config_COMP_MODE_WINDOW      = 1,
    ads1115_config_COMP_MODE_DEFAULT     = ads1115_config_COMP_MODE_TRADITIONAL
} ads1115_config_comp_mode_t;

typedef enum {
    ads1115_config_COMP_POL_ACTIVE_LOW  = 0,
    ads1115_config_COMP_POL_ACTIVE_HIGH = 1,
    ads1115_config_COMP_POL_DEFAULT     = ads1115_config_COMP_POL_ACTIVE_LOW
} ads1115_config_comp_pol_t;

typedef enum {
    ads1115_config_COMP_LAT_NON_LATCHING = 0,
    ads1115_config_COMP_LAT_LATCHING     = 1,
    ads1115_config_COMP_LAT_DEFAULT      = ads1115_config_COMP_LAT_NON_LATCHING
} ads1115_config_comp_lat_t;

typedef enum {
    ads1115_config_COMP_QUE_ASSERT_AFTER_ONE  = 0b00,
    ads1115_config_COMP_QUE_ASSERT_AFTER_TWO  = 0b01,
    ads1115_config_COMP_QUE_ASSERT_AFTER_FOUR = 0b10,
    ads1115_config_COMP_QUE_DISABLE           = 0b11,
    ads1115_config_COMP_QUE_DEFAULT           = ads1115_config_COMP_QUE_DISABLE
} ads1115_config_comp_que_t;

#define ADS1115_CONFIG_BITS_OS        1
#define ADS1115_CONFIG_BITS_MUX       3
#define ADS1115_CONFIG_BITS_PGA       3
#define ADS1115_CONFIG_BITS_MODE      1
#define ADS1115_CONFIG_BITS_DR        3
#define ADS1115_CONFIG_BITS_COMP_MODE 1
#define ADS1115_CONFIG_BITS_COMP_POL  1
#define ADS1115_CONFIG_BITS_COMP_LAT  1
#define ADS1115_CONFIG_BITS_COMP_QUE  2

#define ADS1115_CONFIG_MASK_OS        ((1 << ADS1115_CONFIG_BITS_OS) - 1)
#define ADS1115_CONFIG_MASK_MUX       ((1 << ADS1115_CONFIG_BITS_MUX) - 1)
#define ADS1115_CONFIG_MASK_PGA       ((1 << ADS1115_CONFIG_BITS_PGA) - 1)
#define ADS1115_CONFIG_MASK_MODE      ((1 << ADS1115_CONFIG_BITS_MODE) - 1)
#define ADS1115_CONFIG_MASK_DR        ((1 << ADS1115_CONFIG_BITS_DR) - 1)
#define ADS1115_CONFIG_MASK_COMP_MODE ((1 << ADS1115_CONFIG_BITS_COMP_MODE) - 1)
#define ADS1115_CONFIG_MASK_COMP_POL  ((1 << ADS1115_CONFIG_BITS_COMP_POL) - 1)
#define ADS1115_CONFIG_MASK_COMP_LAT  ((1 << ADS1115_CONFIG_BITS_COMP_LAT) - 1)
#define ADS1115_CONFIG_MASK_COMP_QUE  ((1 << ADS1115_CONFIG_BITS_COMP_QUE) - 1)

typedef struct {
    union {
        struct {
            uint16_t OS : ADS1115_CONFIG_BITS_OS;
            uint16_t MUX : ADS1115_CONFIG_BITS_MUX;
            uint16_t PGA : ADS1115_CONFIG_BITS_PGA;
            uint16_t MODE : ADS1115_CONFIG_BITS_MODE;
            uint16_t DR : ADS1115_CONFIG_BITS_DR;
            uint16_t COMP_MODE : ADS1115_CONFIG_BITS_COMP_MODE;
            uint16_t COMP_POL : ADS1115_CONFIG_BITS_COMP_POL;
            uint16_t COMP_LAT : ADS1115_CONFIG_BITS_COMP_LAT;
            uint16_t COMP_QUE : ADS1115_CONFIG_BITS_COMP_QUE;
        };
        uint16_t raw;
    };
} ads1115_config_register_t;

typedef struct {
    uint16_t value;
} ads1115_lo_thresh_register_t;

typedef struct {
    uint16_t value;
} ads1115_hi_thresh_register_t;

typedef struct {
    ads1115_address_pointer_register_t address;
    union {
        ads1115_conversion_register_t conversion;
        ads1115_config_register_t     config;
        ads1115_lo_thresh_register_t  lo_thresh;
        ads1115_hi_thresh_register_t  hi_thresh;
        uint16_t                      raw;
    };
} ads1115_register_t;

#pragma pack(pop)

void ads1115_log_register(esp_log_level_t                 lvl,
                          ads1115_register_t const *const reg);

void ads1115_encode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_host_reg,
    ads1115_address_pointer_register_t *const       out_i2c_reg);

void ads1115_decode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_i2c_reg,
    ads1115_address_pointer_register_t *const       out_host_reg);

void ads1115_encode_register(ads1115_register_t const *const in_host_reg,
                             ads1115_register_t *const       out_i2c_reg);

void ads1115_decode_register(ads1115_register_t const *const in_i2c_reg,
                             ads1115_register_t *const       out_host_reg);

#ifdef __cplusplus
}
#endif
