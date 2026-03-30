#include "ads1115.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "sdkconfig.h"
#include <esp_err.h>
#include <math.h>
#include <stdint.h>

static const char *const TAG = "ads1115";

#define I2C_MASTER_FREQ_HZ 400000

static uint8_t endian_swap_8_host_little_endian(uint8_t val)
{
    uint8_t ret = (val & 0x1) << 7 | (val & 0x2) << 5 | (val & 0x4) << 3 |
                  (val & 0x8) << 1 | (val & 0x10) >> 1 | (val & 0x20) >> 3 |
                  (val & 0x40) >> 5 | (val & 0x80) >> 7;
    ESP_LOGD(TAG,
             "endian_swap_8_host_little_endian: input=0x%02x, output=0x%02x",
             val, ret);
    return ret;
}

static uint16_t endian_swap_16_host_little_endian(uint16_t val)
{
    uint8_t first_byte         = val & 0xFF;
    uint8_t swapped_first_byte = endian_swap_8_host_little_endian(first_byte);
    uint8_t second_byte         = (val >> 8) & 0xFF;
    uint8_t swapped_second_byte = endian_swap_8_host_little_endian(second_byte);
    
    ESP_LOGD(TAG, "first byte (before swap) : 0x%02x", first_byte);
    ESP_LOGD(TAG, "first byte (after swap) : 0x%02x", swapped_first_byte);
    ESP_LOGD(TAG, "second byte (before swap) : 0x%02x", second_byte);
    ESP_LOGD(TAG, "second byte (after swap) : 0x%02x", swapped_second_byte);

    uint16_t ret = (uint16_t)swapped_first_byte << 8 | (uint16_t)swapped_second_byte;
    ESP_LOGD(TAG,
             "endian_swap_16_host_little_endian: input=0x%04x, output=0x%04x",
             val, ret);
    return ret;
}

typedef void (*encode_fn_t)(ads1115_register_t const *const reg,
                            ads1115_register_t *const       reg_out);
typedef void (*decode_fn_t)(ads1115_register_t const *const reg,
                            ads1115_register_t *const       reg_out);

typedef union
{
    struct
    {
        uint8_t P : 2;
        uint8_t RESERVED : 6;
    };
    uint8_t val;
} encoded_address_pointer_register_t;

typedef union
{
    struct
    {
        uint16_t COMP_QUE : 2;
        uint16_t COMP_LAT : 1;
        uint16_t COMP_POL : 1;
        uint16_t COMP_MODE : 1;
        uint16_t DR : 3;
        uint16_t MODE : 1;
        uint16_t PGA : 3;
        uint16_t MUX : 3;
        uint16_t OS : 1;
    };
    uint16_t raw;
} encoded_config_register_t;

void ads1115_register_encode_host_little_endian(
    ads1115_register_t const *const reg, ads1115_register_t *const reg_out)
{
    encoded_address_pointer_register_t encoded_address = {0};
    encoded_address.P                                  = reg->address.P;

    reg_out->address.val = *((uint8_t *)&encoded_address);
    switch (reg->address.P)
    {
    case ADS1115_REG_CONVERSION:
    case ADS1115_REG_LO_THRESH:
    case ADS1115_REG_HI_THRESH:
        reg_out->reg.conversion.conversion_result =
            endian_swap_16_host_little_endian(
                reg->reg.conversion.conversion_result);
        break;

    case ADS1115_REG_CONFIG:
    {
        encoded_config_register_t encoded_config = {0};
        encoded_config.OS                        = reg->reg.config.OS;
        encoded_config.MUX                       = reg->reg.config.MUX;
        encoded_config.PGA                       = reg->reg.config.PGA;
        encoded_config.MODE                      = reg->reg.config.MODE;
        encoded_config.DR                        = reg->reg.config.DR;
        encoded_config.COMP_MODE                 = reg->reg.config.COMP_MODE;
        encoded_config.COMP_POL                  = reg->reg.config.COMP_POL;
        encoded_config.COMP_LAT                  = reg->reg.config.COMP_LAT;
        encoded_config.COMP_QUE                  = reg->reg.config.COMP_QUE;

        reg_out->reg.raw = encoded_config.raw;
        break;
    }

    default:
        reg_out->reg.raw = reg->reg.raw;
        break;
    }
}

void ads1115_register_decode_host_little_endian(
    ads1115_register_t const *const reg, ads1115_register_t *const reg_out)
{
    encoded_address_pointer_register_t *encoded_address =
        (encoded_address_pointer_register_t *)&reg->address.val;
    ads1115_address_pointer_register_t decoded_address = {0};
    decoded_address.P                                  = encoded_address->P;

    reg_out->address.val = *((uint8_t *)&decoded_address);

    switch (decoded_address.P)
    {
    case ADS1115_REG_CONVERSION:
    case ADS1115_REG_LO_THRESH:
    case ADS1115_REG_HI_THRESH:
        reg_out->reg.raw = 
            endian_swap_16_host_little_endian(
                reg->reg.conversion.conversion_result);
        break;

    case ADS1115_REG_CONFIG:
    {
        encoded_config_register_t *encoded_config =
            (encoded_config_register_t *)&reg->reg.raw;
        ads1115_config_register_t decoded_config = {0};
        decoded_config.OS                        = encoded_config->OS;
        decoded_config.MUX                       = encoded_config->MUX;
        decoded_config.PGA                       = encoded_config->PGA;
        decoded_config.MODE                      = encoded_config->MODE;
        decoded_config.DR                        = encoded_config->DR;
        decoded_config.COMP_MODE                 = encoded_config->COMP_MODE;
        decoded_config.COMP_POL                  = encoded_config->COMP_POL;
        decoded_config.COMP_LAT                  = encoded_config->COMP_LAT;
        decoded_config.COMP_QUE                  = encoded_config->COMP_QUE;

        reg_out->reg.raw = decoded_config.raw;
        break;
    }

    default:
        reg_out->reg.raw = reg->reg.raw;
        break;
    }
}

static encode_fn_t ads1115_register_encode =
    &ads1115_register_encode_host_little_endian;
static decode_fn_t ads1115_register_decode =
    &ads1115_register_decode_host_little_endian;

void ads1115_log_register(esp_log_level_t                 lvl,
                          ads1115_register_t const *const reg)
{
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "Register dump %p:", reg);
    if (reg == NULL)
    {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "<NULL register pointer>");
        return;
    }

    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "address.val = 0x%02x", reg->address.val);
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "address.P = %02x", reg->address.P);
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "raw value = 0x%04x", reg->reg.raw);
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "raw(MSB) : 0x%02x",
                        (reg->reg.raw >> 8) & 0xFF);
    ESP_LOG_LEVEL_LOCAL(lvl, TAG, "raw(LSB) : 0x%02x", reg->reg.raw & 0xFF);
    switch (reg->address.P)
    {
    case ADS1115_REG_CONVERSION:
    {
        ESP_LOGD(TAG, "reg_id............... : ADS1115_REG_CONVERSION");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  convesion_result... : 0x%02x",
                            reg->reg.conversion.conversion_result);
        break;
    }

    case ADS1115_REG_CONFIG:
    {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id........ : ADS1115_REG_CONFIG");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  OS.......... : %u",
                            reg->reg.config.OS);
        switch (reg->reg.config.OS)
        {
        case ads1115_config_OS_WRITE_NO_EFFECT:
            ESP_LOG_LEVEL_LOCAL(
                lvl, TAG, "    WRITE:NO_EFFECT / READ:CONVERSION_IN_PROGRESS");
            break;
        case ads1115_config_OS_WRITE_START_SINGLE_CONVERSION:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG,
                                "    WRITE:START_SINGLE_CONVERSION / "
                                "READ:CONVERSION_NOT_IN_PROGRESS");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  MUX......... : %u",
                            reg->reg.config.MUX);
        switch (reg->reg.config.MUX)
        {
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

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  PGA......... : %u",
                            reg->reg.config.PGA);
        switch (reg->reg.config.PGA)
        {
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
        case ads1115_config_PGA_RESERVED1:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 0.256V");
            break;
        case ads1115_config_PGA_RESERVED2:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    +/- 0.256V");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  MODE........ : %u",
                            reg->reg.config.MODE);
        switch (reg->reg.config.MODE)
        {
        case ads1115_config_MODE_CONTINUOUS:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Continuous conversion mode");
            break;
        case ads1115_config_MODE_SINGLE_SHOT:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Single-shot conversion mode");
            break;
        default:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  DR.......... : %u",
                            reg->reg.config.DR);
        switch (reg->reg.config.DR)
        {
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

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  COMP_MODE... : %u",
                            reg->reg.config.COMP_MODE);
        switch (reg->reg.config.COMP_MODE)
        {
        case ads1115_config_COMP_MODE_TRADITIONAL:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Traditional");
            break;
        case ads1115_config_COMP_MODE_WINDOW:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Window");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  COMP_POL.... : %u",
                            reg->reg.config.COMP_POL);
        switch (reg->reg.config.COMP_POL)
        {
        case ads1115_config_COMP_POL_ACTIVE_LOW:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Active LOW");
            break;
        case ads1115_config_COMP_POL_ACTIVE_HIGH:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Active HIGH");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  COMP_LAT.... : %u",
                            reg->reg.config.COMP_LAT);
        switch (reg->reg.config.COMP_LAT)
        {
        case ads1115_config_COMP_LAT_NON_LATCHING:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Non-Latching");
            break;
        case ads1115_config_COMP_LAT_LATCHING:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Latching");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }

        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  COMP_QUE.... : %u",
                            reg->reg.config.COMP_QUE);
        switch (reg->reg.config.COMP_QUE)
        {
        case ads1115_config_COMP_QUE_ASSERT_AFTER_ONE:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 1");
            break;
        case ads1115_config_COMP_QUE_ASSERT_AFTER_TWO:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 2");
            break;
        case ads1115_config_COMP_QUE_ASSERT_AFTER_FOUR:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Assert after 3");
            break;
        case ads1115_config_COMP_QUE_DISABLE:
            ESP_LOG_LEVEL_LOCAL(lvl, TAG, "    Disabled");
            break;
        default:
            ESP_LOGW(TAG, "    <unknown>");
        }
        break;
    }

    case ADS1115_REG_LO_THRESH:
    {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id.... : ADS1115_REG_LO_THRESH");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  value... : %hu",
                            reg->reg.lo_thresh.value);
        break;
    }

    case ADS1115_REG_HI_THRESH:
    {
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "reg_id.... : ADS1115_REG_HI_THRESH");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  value... : %hu",
                            reg->reg.hi_thresh.value);
        break;
    }

    default:
        ESP_LOGW(TAG, "Unknown register id");
    }
}

float ads1115_get_voltage(ads1115_config_pga_t                       pga,
                          ads1115_conversion_register_t const *const conversion)
{
    float FS = 0.f;
    switch (pga)
    {
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
    case ads1115_config_PGA_RESERVED1:
    case ads1115_config_PGA_RESERVED2:
        FS = 0.256f;
        break;
    default:
        return NAN;
    }

    float val = FS * (int16_t)conversion->conversion_result / (1 << 15);
    ESP_LOGD(TAG, "%f * %d / (1<<15) = %f", FS,
             (int16_t)conversion->conversion_result, val);
    return val;
}

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle)
{
    i2c_device_config_t ads1115_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ads1115_i2c_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ};

    esp_err_t ret =
        i2c_master_bus_add_device(bus, &ads1115_dev_config, ads1115_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add ADS1115 device to I2C bus: %s",
                 esp_err_to_name(ret));
        return ret;
    }

#if 0
    // Send a reset command to the ADS1115 as i2c cmd 0x06 (General Call Reset)
    // to ensure it's in a known state.
    ESP_LOGD(TAG, "Sending reset command (0x06) to ADS1115 to ensure it's in a "
                  "known state");
    uint8_t reset_cmd = 0x06;
    ret               = i2c_master_transmit(*ads1115_dev_handle, &reset_cmd,
                                            sizeof(reset_cmd), 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send reset command to ADS1115: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // ADS1115

    ESP_LOGD(TAG,
             "Reading ADS1115 config register to verify communication with "
             "the sensor");
    ads1115_register_t read_config = {.address.P = ADS1115_REG_CONFIG};
    read_config.reg.raw =
        0xbeef; // set to a known invalid value to ensure the read is working
    ESP_ERROR_CHECK(ads1115_read_register(*ads1115_dev_handle, &read_config));
    ads1115_log_register(ESP_LOG_DEBUG, &read_config);
#endif

    return ESP_OK;
}

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg)
{
    if (reg == NULL)
    {
        ESP_LOGE(TAG, "Cannot read register: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Reading register with address pointer 0x%02x",
             reg->address.val);

    ads1115_register_t reg_encoded;
    ads1115_register_encode(reg, &reg_encoded);

    ESP_LOGD(TAG, "Transmitting address pointer (encoded): 0x%02x",
             reg_encoded.address.val);

    esp_err_t ret = i2c_master_transmit_receive(
        ads1115_dev_handle, (uint8_t const *)&reg_encoded.address,
        sizeof(ads1115_address_pointer_register_t), (uint8_t *)&reg_encoded.reg,
        sizeof(reg_encoded.reg), 100);

    ESP_LOGD(TAG, "Reecived (encoded): 0x%04x", reg_encoded.reg.raw);

    ads1115_register_decode(&reg_encoded, reg);

    ESP_LOGD(TAG, "Received (decoded): 0x%04x", reg->reg.raw);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read register: %s", esp_err_to_name(ret));
        ads1115_log_register(ESP_LOG_ERROR, reg);
    }

    return ret;
}

esp_err_t ads1115_write_register(i2c_master_dev_handle_t   ads1115_dev_handle,
                                 const ads1115_register_t *reg)
{
    if (reg == NULL)
    {
        ESP_LOGE(TAG, "Cannot write register: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    // ESP32 is little endian, but ADS1115 expects big endian. Convert the raw
    // value to big endian before transmission.
    ads1115_register_t reg_be;
    ads1115_register_encode(reg, &reg_be);

    ESP_LOGD(TAG, "Writing register (host): 0x%04x", reg->reg.raw);
    ESP_LOGD(TAG, "Writing register (encoded): 0x%04x", reg_be.reg.raw);

    esp_err_t ret =
        i2c_master_transmit(ads1115_dev_handle, (uint8_t const *)&reg_be,
                            sizeof(ads1115_register_t), 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write register: %s", esp_err_to_name(ret));
        ads1115_log_register(ESP_LOG_ERROR, reg);
    }

    return ret;
}
