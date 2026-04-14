#pragma once

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log_level.h"
#include <stdint.h>

#if !defined(ADS1115_SENSOR_ADDR)
#define ADS1115_SENSOR_ADDR 0x48 /*!< Address of the ADS1115 sensor */
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
    ads1115_config_OS_READ_CONVERSION_NOT_IN_PROGRESS = 1
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

/**
 * @defgroup ADS1115_Config_Masks ADS1115 Config Masks
 *
 * @brief Bit masks for the ADS1115 configuration register fields. Assumes that
 * the field values are right-aligned, i.e., they occupy the least significant
 * bits of their respective field widths.
 *
 * @{
 */
#define ADS1115_CONFIG_MASK_OS        ((1 << ADS1115_CONFIG_BITS_OS) - 1)
#define ADS1115_CONFIG_MASK_MUX       ((1 << ADS1115_CONFIG_BITS_MUX) - 1)
#define ADS1115_CONFIG_MASK_PGA       ((1 << ADS1115_CONFIG_BITS_PGA) - 1)
#define ADS1115_CONFIG_MASK_MODE      ((1 << ADS1115_CONFIG_BITS_MODE) - 1)
#define ADS1115_CONFIG_MASK_DR        ((1 << ADS1115_CONFIG_BITS_DR) - 1)
#define ADS1115_CONFIG_MASK_COMP_MODE ((1 << ADS1115_CONFIG_BITS_COMP_MODE) - 1)
#define ADS1115_CONFIG_MASK_COMP_POL  ((1 << ADS1115_CONFIG_BITS_COMP_POL) - 1)
#define ADS1115_CONFIG_MASK_COMP_LAT  ((1 << ADS1115_CONFIG_BITS_COMP_LAT) - 1)
#define ADS1115_CONFIG_MASK_COMP_QUE  ((1 << ADS1115_CONFIG_BITS_COMP_QUE) - 1)
/** @} */ // End of ADS1115_Config_Masks

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

/**
 * @defgroup ADS1115_Logging ADS1115 Logging
 *
 * @brief Functions for logging the contents of ADS1115 register structures in a
 * human-readable format.
 *
 * @details Uses ESP-IDF's logging facilities to output the register contents at
 * various log levels, i.e., ESP_LOG_LEVEL_LOCAL(lvl, TAG, ...).
 *
 * Assumes that the input register structure has already been decoded from I2C
 * wire format if necessary, i.e., the fields of the register structure are in
 * host byte order and properly parsed into their respective bitfields.
 *
 * @{
 */

/**
 * @brief Log the contents of an ADS1115 register.
 *
 * @param lvl The log level.
 * @param reg Pointer to the register structure to log.
 */
void ads1115_log_register(esp_log_level_t                 lvl,
                          ads1115_register_t const *const reg);

/** @} */ // End of ADS1115_Logging

/**
 * @defgroup ADS1115_Register_Encoding ADS1115 Register Encoding
 *
 * @brief Functions for encoding and decoding ADS1115 register structures to and
 * from I2C wire format.
 * @{
 */

/**
 * @brief Encode an ADS1115 address register pointer structure into I2C wire
 * format.
 *
 * @param in_host_reg Pointer to the input address register pointer structure in
 * host format.
 * @param out_i2c_reg Pointer to the output address register pointer structure
 * in I2C wire format.
 */
void ads1115_encode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_host_reg,
    ads1115_address_pointer_register_t *const       out_i2c_reg);

/**
 * @brief Decode an ADS1115 address register pointer structure from I2C wire
 * format.
 *
 * @param in_i2c_reg Pointer to the input address register pointer structure in
 * I2C wire format.
 * @param out_host_reg Pointer to the output address register pointer structure
 * in host format.
 */
void ads1115_decode_address_register_pointer(
    ads1115_address_pointer_register_t const *const in_i2c_reg,
    ads1115_address_pointer_register_t *const       out_host_reg);

/**
 * @brief Encode an ADS1115 register structure into I2C wire format for writing
 * to the device.
 *
 * @param in_host_reg Pointer to the input register structure in host format.
 * @param out_i2c_reg Pointer to the output register structure in I2C wire
 * format.
 */
void ads1115_encode_register(ads1115_register_t const *const in_host_reg,
                             ads1115_register_t *const       out_i2c_reg);

/**
 * @brief Decode a raw register value from I2C wire format into an ADS1115
 * register structure.
 *
 * @param in_i2c_reg Pointer to the input register structure in I2C wire format.
 * @param out_host_reg Pointer to the output register structure in host format.
 */
void ads1115_decode_register(ads1115_register_t const *const in_i2c_reg,
                             ads1115_register_t *const       out_host_reg);

/** @} */ // End of ADS1115_Register_Encoding

/**
 * @defgroup ADS1115_I2C_v2 ADS1115 Interface over I2C v2 Driver
 * @{
 */

/**
 * @brief Calculate the voltage corresponding to a given raw conversion register
 * value and PGA setting.
 *
 * @param pga The current PGA setting of the ADS1115.
 * @param conversion A pointer to the conversion register containing the raw ADC
 * conversion result.
 *
 * @return The voltage corresponding to the raw conversion result, in volts.
 */
float ads1115_get_voltage(
    ads1115_config_pga_t                       pga,
    ads1115_conversion_register_t const *const conversion);

/**
 * @brief Add an ADS1115 device to the I2C bus.
 *
 * @param bus The I2C bus handle.
 * @param ads1115_i2c_addr The I2C address of the ADS1115 device.
 * @param ads1115_dev_handle Pointer to the device handle to be initialized.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle);

/**
 * @brief Read a register from the ADS1115 device.
 *
 * @param ads1115_dev_handle The device handle of the ADS1115.
 * @param reg Pointer to the register structure to store the read value.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg);

/**
 * @brief Write a register to the ADS1115 device.
 *
 * @param ads1115_dev_handle The device handle of the ADS1115.
 * @param reg Pointer to the register structure containing the value to write.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t ads1115_write_register(i2c_master_dev_handle_t   ads1115_dev_handle,
                                 const ads1115_register_t *reg);

/**
 * @brief Get a single conversion result from the ADS1115 device.
 *
 * @param dev_handle The device handle of the ADS1115.
 * @param output Pointer to store the conversion result.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t ads1115_get_single_conversion(i2c_master_dev_handle_t dev_handle,
                                        int16_t                *output);

/**
 * @brief Enable the conversion ready interrupt for the ADS1115 device.
 *
 * @details Sets the most significant bit of HI_THRESH register to 0b1, the
 * most significant bit of LO_THRESH register to 0b0, and the COMP_QUE bits
 * in the CONFIG register to 0b00. This configures the ADS1115 to assert the
 * ALERT/RDY pin when a conversion is ready.
 *
 * @param dev_handle The device handle of the ADS1115.
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t
ads1115_enable_conversion_ready_interrupt(i2c_master_dev_handle_t dev_handle,
                                          const uint8_t ads1115_i2c_addr);

/** @} */ // End of ADS1115_I2C_v2