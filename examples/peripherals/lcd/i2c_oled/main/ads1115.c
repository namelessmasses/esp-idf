#include "ads1115.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "sdkconfig.h"
#include <esp_err.h>
#include <stdint.h>

static const char *const TAG = "ads1115";

#define I2C_MASTER_FREQ_HZ 400000

void ads1115_log_register(esp_log_level_t                 lvl,
                          ads1115_register_t const *const reg)
{
    switch (reg->address.P)
    {
    case ADS1115_REG_CONVERSION:
    {
        ESP_LOGD(TAG, "reg_id............... : ADS1115_REG_CONVERSION");
        ESP_LOG_LEVEL_LOCAL(lvl, TAG, "  convesion_result... : %hu",
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

        ESP_LOGW(TAG, "  DR.......... : %u", reg->reg.config.DR);
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

esp_err_t ads1115_bus_add_device(i2c_master_bus_handle_t  bus,
                                 const uint8_t            ads1115_i2c_addr,
                                 i2c_master_dev_handle_t *ads1115_dev_handle)
{
    i2c_device_config_t ads1115_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ads1115_i2c_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ};

    return i2c_master_bus_add_device(bus, &ads1115_dev_config,
                                     ads1115_dev_handle);
}

esp_err_t ads1115_read_register(i2c_master_dev_handle_t ads1115_dev_handle,
                                ads1115_register_t     *reg)
{
    esp_err_t ret =
        i2c_master_transmit_receive(ads1115_dev_handle, (uint8_t const *)&reg->address,
                                    sizeof(ads1115_address_pointer_register_t),
                                    (uint8_t *)&reg->reg, sizeof(reg->reg), 100);
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
    esp_err_t ret =
        i2c_master_transmit(ads1115_dev_handle, (uint8_t const *)reg,
                            sizeof(ads1115_register_t), 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write register: %s", esp_err_to_name(ret));
        ads1115_log_register(ESP_LOG_ERROR, reg);
    }

    return ret;
}