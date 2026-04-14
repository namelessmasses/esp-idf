#include "freertos/FreeRTOS.h"

#include "sht41.h"

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "sht41";

#if 0
#define I2C_MASTER_SCL_IO                                                      \
    CONFIG_I2C_MASTER_SCL /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO                                                      \
    CONFIG_I2C_MASTER_SDA        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ                                                     \
    CONFIG_I2C_MASTER_FREQUENCY     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#endif

#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_MASTER_FREQ_HZ    400000

esp_err_t sht41_bus_add_device(i2c_master_bus_handle_t bus, const uint8_t sht41_i2c_addr,
                               i2c_master_dev_handle_t *sht41_dev_handle)
{
    i2c_device_config_t sht41_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = sht41_i2c_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    return i2c_master_bus_add_device(bus, &sht41_dev_config, sht41_dev_handle);
}

const char *sht41_get_cmd_string(sht41_command_t cmd)
{
    switch (cmd)
    {
    case CMD_READ_HIGH_PRECISION:
        return "High Precision";
    case CMD_READ_MEDIUM_PRECISION:
        return "Medium Precision";
    case CMD_READ_LOW_PRECISION:
        return "Low Precision";
    case CMD_READ_SERIAL_NUMBER:
        return "Read Serial Number";
    case CMD_HEATED_200mW_1000ms:
        return "Heated 200mW 1000ms";
    case CMD_HEATED_200mW_100ms:
        return "Heated 200mW 100ms";
    case CMD_HEATED_110mW_1000ms:
        return "Heated 110mW 1000ms";
    case CMD_HEATED_110mW_100ms:
        return "Heated 110mW 100ms";
    case CMD_HEATED_20mW_1000ms:
        return "Heated 20mW 1000ms";
    case CMD_HEATED_20mW_100ms:
        return "Heated 20mW 100ms";
    default:
        return "Unknown Command";
    }
}

void sht41_print_data(const sht41_data_t *data)
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

esp_err_t sht41_get_reading(i2c_master_dev_handle_t dev_handle,
                            sht41_command_t command, sht41_data_t *data,
                            uint32_t wait_duration_ms)
{
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, (uint8_t const *)&command,
                                        1, I2C_MASTER_TIMEOUT_MS));

    vTaskDelay(pdMS_TO_TICKS(wait_duration_ms));

    uint8_t   raw_data[6];
    esp_err_t res = ESP_ERR_TIMEOUT;
    for (int i = 0; i < 3; i++)
    {
        res = i2c_master_receive(dev_handle, raw_data, sizeof(raw_data),
                                 I2C_MASTER_TIMEOUT_MS);
        if (res == ESP_OK)
        {
            break;
        }

        if (res == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(TAG, "I2C bus is busy, retrying in %ums...",
                     wait_duration_ms);
        }
        else
        {
            ESP_LOGW(TAG, "Error reading from sensor: %s, retrying in %ums...",
                     esp_err_to_name(res), wait_duration_ms);
        }

        vTaskDelay(pdMS_TO_TICKS(wait_duration_ms));
    }

    if (res != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "Failed to read data from sensor after multiple attempts");
        return res;
    }

    process_sht41_data(raw_data, data);

    return ESP_OK;
}

esp_err_t sht41_get_reading_with_heating(i2c_master_dev_handle_t dev_handle,
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

    return sht41_get_reading(dev_handle, command, data, wait_duration_ms);
}

#if 0
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

    while (1)
    {
        sht41_command_t command[] = {CMD_READ_HIGH_PRECISION,
                                     CMD_READ_MEDIUM_PRECISION,
                                     CMD_READ_LOW_PRECISION};

        for (uint32_t cmd = 0; cmd < 3; ++cmd)
        {
            ESP_LOGI(TAG, "Requesting %s measurement from SHT41 sensor",
                     get_cmd_string(command[cmd]));
            sht41_data_t response_data;
            ESP_ERROR_CHECK(sensor_get_reading(dev_handle, command[cmd],
                                               &response_data, 10));
            print_sht41_data(&response_data);
        }

#if 0
    ESP_LOGI(TAG, "Requesting measurement with heating from SHT41 sensor");
    heating_config_t heating_config = {.power = POWER_110mW, .duration = LONG};
    ESP_ERROR_CHECK(sensor_get_reading(dev_handle, &data_struct, &heating_config));
    print_sht41_data(&data_struct);
#endif
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif
