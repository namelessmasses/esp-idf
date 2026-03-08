/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected
   over I2C.

   The sensor used in this example is a SHT41 inertial measurement unit.
*/
#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO                                                      \
    CONFIG_I2C_MASTER_SCL /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO                                                      \
    CONFIG_I2C_MASTER_SDA        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ                                                     \
    CONFIG_I2C_MASTER_FREQUENCY     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS     1000

#define SHT41_SENSOR_ADDR 0x44 /*!< Address of the SHT41 sensor */
enum
{
    CMD_READ_HIGH_PRECISION   = 0xFD,
    CMD_READ_MEDIUM_PRECISION = 0xF6,
    CMD_READ_LOW_PRECISION    = 0xE0,
    CMD_READ_SERIAL_NUMBER    = 0x89,
    CMD_HEATED_200mW_1000ms   = 0x39,
    CMD_HEATED_200mW_100ms    = 0x32,
    CMD_HEATED_110mW_1000ms   = 0x2F,
    CMD_HEATED_110mW_100ms    = 0x24,
    CMD_HEATED_20mW_1000ms    = 0x1E,
    CMD_HEATED_20mW_100ms     = 0x15
} sht41_command_t;

/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle,
                            i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port                     = I2C_MASTER_NUM,
        .sda_io_num                   = I2C_MASTER_SDA_IO,
        .scl_io_num                   = I2C_MASTER_SCL_IO,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SHT41_SENSOR_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

typedef struct
{
    uint16_t temp_ticks;
    float    temperature_celcius;
    float    temperature_fahrenheit;
    uint16_t rh_ticks;
    float    relative_humidity;
} sht41_data_t;

static void print_sht41_data(const sht41_data_t *data)
{
    ESP_LOGI(TAG, "Raw temperature ticks: %u", data->temp_ticks);
    ESP_LOGI(TAG, "Temperature: %.2f °C", data->temperature_celcius);
    ESP_LOGI(TAG, "Temperature: %.2f °F", data->temperature_fahrenheit);
    ESP_LOGI(TAG, "Raw humidity ticks: %u", data->rh_ticks);
    ESP_LOGI(TAG, "Relative Humidity: %.2f %%", data->relative_humidity);
}

static void process_sht41_data(const uint8_t *raw_data, sht41_data_t *data)
{
    data->temp_ticks = raw_data[0] << 8 | raw_data[1];
    data->temperature_celcius =
        -45.f + 175.f * ((float)data->temp_ticks / 65535.f);
    data->temperature_fahrenheit =
        -49.f + 315.f * ((float)data->temp_ticks / 65535.f);

    data->rh_ticks          = raw_data[3] << 8 | raw_data[4];
    data->relative_humidity = -6.f + 125.f * ((float)data->rh_ticks / 65535.f);
}

typedef struct
{
    enum
    {
        POWER_20mW,
        POWER_110mW,
        POWER_200mW
    } power;

    enum
    {
        SHORT = 100,
        LONG  = 1000
    } duration;
} heating_config_t;

static esp_err_t sensor_get_reading(i2c_master_dev_handle_t dev_handle,
                                    sht41_data_t           *data,
                                    const heating_config_t *heating_config)
{
    uint8_t  command          = CMD_READ_HIGH_PRECISION;
    uint16_t wait_duration_ms = 10;

    if (heating_config)
    {
        if (heating_config->power == POWER_20mW)
        {
            if (heating_config->duration == LONG)
            {
                wait_duration_ms += 1000;
                command = CMD_HEATED_20mW_1000ms;
            }
            else
            {
                wait_duration_ms += 100;
                command = CMD_HEATED_20mW_100ms;
            }
        }
        else if (heating_config->power == POWER_110mW)
        {
            if (heating_config->duration == LONG)
            {
                wait_duration_ms += 1000;
                command = CMD_HEATED_110mW_1000ms;
            }
            else
            {
                wait_duration_ms += 100;
                command = CMD_HEATED_110mW_100ms;
            }
        }
        else
        {
            if (heating_config->duration == LONG)
            {
                wait_duration_ms += 1000;
                command = CMD_HEATED_200mW_1000ms;
            }
            else
            {
                wait_duration_ms += 100;
                command = CMD_HEATED_200mW_100ms;
            }
        }
    }

    ESP_ERROR_CHECK(
        i2c_master_transmit(dev_handle, &command, 1, I2C_MASTER_TIMEOUT_MS));

    vTaskDelay(pdMS_TO_TICKS(wait_duration_ms));

    uint8_t raw_data[6];
    esp_err_t res = ESP_ERR_TIMEOUT;
    for (int i = 0; i < 3; i++)
    {
        res = i2c_master_receive(
            dev_handle, raw_data, sizeof(raw_data), I2C_MASTER_TIMEOUT_MS);
        if (res == ESP_OK)
        {
            break;
        }

        if (res == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(TAG, "I2C bus is busy, retrying in 10ms...");
        }
        else
        {
            ESP_LOGW(TAG, "Error reading from sensor: %s, retrying in 10ms...",
                     esp_err_to_name(res));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read data from sensor after multiple attempts");
        return res;
    }

    process_sht41_data(raw_data, data);

    return ESP_OK;
}

void app_main(void)
{
    uint8_t                 data[6];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_LOGI(TAG, "Probing SHT41 sensor");
    esp_err_t ret = !ESP_OK;
    do
    {
        ret = i2c_master_probe(bus_handle, SHT41_SENSOR_ADDR, 1000);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (ret != ESP_OK);

    ESP_LOGI(TAG, "Requesting serial number from SHT41 sensor");
    uint8_t cmd_read_serial_number = CMD_READ_SERIAL_NUMBER;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &cmd_read_serial_number, 1,
                                        I2C_MASTER_TIMEOUT_MS));

    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "Reading serial number from SHT41 sensor");
    ESP_ERROR_CHECK(i2c_master_receive(dev_handle, data, sizeof(data),
                                       I2C_MASTER_TIMEOUT_MS));

    uint32_t serial_number =
        (data[0] << 24) | (data[1] << 16) | (data[3] << 8) | data[4];

    ESP_LOGI(TAG, "SHT41 Serial Number: 0x%08X", serial_number);

    ESP_LOGI(TAG, "Requesting measurement from SHT41 sensor");
    sht41_data_t data_struct;
    ESP_ERROR_CHECK(sensor_get_reading(dev_handle, &data_struct, NULL));
    print_sht41_data(&data_struct);

    ESP_LOGI(TAG, "Requesting measurement with heating from SHT41 sensor");
    heating_config_t heating_config = {.power = POWER_110mW, .duration = LONG};
    ESP_ERROR_CHECK(sensor_get_reading(dev_handle, &data_struct, &heating_config));
    print_sht41_data(&data_struct);
}
