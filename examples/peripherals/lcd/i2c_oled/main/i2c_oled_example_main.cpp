/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "./kiwi/I2CFactory.hpp"
#include "./kiwi/II2C.hpp"
#include "./kiwi/personal/ads1115/ADS1115.hpp"
#include "./kiwi/personal/sht41/SHT41.hpp"
#include <stdint.h>

extern "C" {
#include <freertos/FreeRTOS.h>

#include <driver/i2c_types.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_log_level.h>
#include <esp_timer.h>
#include <freertos/task.h>

#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>
}

#include <atomic>
#include <cmath>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

static const char *TAG = "example";

static const uint32_t EXAMPLE_I2C_BUS_PORT = 0;
static const uint32_t EXAMPLE_PIN_NUM_SDA  = 21;
static const uint32_t EXAMPLE_PIN_NUM_SCL  = 22;
static const uint32_t EXAMPLE_PIN_NUM_RST  = -1;

static const uint32_t sc_poll_sensors_period_ms = 1000;

typedef struct {
    std::shared_ptr<kiwi::i2c::II2C>    i2c_bus;
    std::unique_ptr<kiwi::i2c::ADS1115> p_ADS1115;
    std::unique_ptr<kiwi::i2c::SHT41>   p_SHT41;
} poll_sensors_arg_t;

static constexpr float R1 = 983.f;
static constexpr float R2 = 323.f;

static struct {
    kiwi::i2c::SHT41::Reading temp_humid_data[2];
    float                     voltage_data[2];
    std::atomic_uint32_t      index = -1;
} s_sensor_data;

static poll_sensors_arg_t s_poll_sensors_arg = {
    .i2c_bus = nullptr, .p_ADS1115 = nullptr, .p_SHT41 = nullptr};

static void poll_sensors(void *arg) {
    poll_sensors_arg_t *poll_arg = (poll_sensors_arg_t *)arg;

    int32_t data_index = s_sensor_data.index.load();
    ++data_index;
    data_index &= 1; // toggle between 0 and 1

    kiwi::i2c::SHT41::Reading reading = poll_arg->p_SHT41->GetReading();
    if (std::isnan(reading.relative_humidity)) {
        ESP_LOGW(TAG,
                 "Failed to read from SHT41 sensor - relative humidity is NaN");
        goto ReadVoltage;
    }

    if (std::isnan(reading.temperature_celcius)) {
        ESP_LOGW(TAG, "Failed to read from SHT41 sensor - temperature is NaN");
        goto ReadVoltage;
    }

    if (std::isnan(reading.temperature_fahrenheit)) {
        ESP_LOGW(TAG, "Failed to read from SHT41 sensor - temperature is NaN");
        goto ReadVoltage;
    }

    s_sensor_data.temp_humid_data[data_index] = reading;

ReadVoltage:

    s_sensor_data.voltage_data[data_index] = poll_arg->p_ADS1115->GetVoltage();

    ESP_LOGI(TAG,
             "Sensor readings updated: voltage=%.2f V",
             s_sensor_data.voltage_data[data_index]);

    s_sensor_data.index.store(data_index);
}

extern "C" void ui_run(i2c_master_bus_handle_t bus_handle);

extern "C" void app_main(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    kiwi::i2c::factory::I2CDriver driver_type =
        kiwi::i2c::factory::I2CDriver::New;

    ESP_LOGI(TAG, "Initializing I2C bus");
    s_poll_sensors_arg.i2c_bus =
        kiwi::i2c::factory::CreateI2C(driver_type,
                                      EXAMPLE_I2C_BUS_PORT,
                                      EXAMPLE_PIN_NUM_SDA,
                                      EXAMPLE_PIN_NUM_SCL);

    if (s_poll_sensors_arg.i2c_bus == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return;
    }

    ESP_LOGI(TAG, "Adding ADS1115 to the I2C bus");
    s_poll_sensors_arg.p_ADS1115 =
        std::make_unique<kiwi::i2c::ADS1115>(s_poll_sensors_arg.i2c_bus);

    s_poll_sensors_arg.p_ADS1115->SetVoltageDivider(R1, R2);

    ESP_LOGI(TAG, "Adding SHT41 to the I2C bus");
    s_poll_sensors_arg.p_SHT41 =
        std::make_unique<kiwi::i2c::SHT41>(s_poll_sensors_arg.i2c_bus);

    ESP_LOGI(TAG,
             "Creating timer to poll sensors every %u ms",
             sc_poll_sensors_period_ms);
    esp_timer_create_args_t poll_sensors_timer_args = {
        .callback              = poll_sensors,
        .arg                   = &s_poll_sensors_arg,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "poll_sensors_timer",
        .skip_unhandled_events = true};
    esp_timer_handle_t poll_sensors_timer;
    ESP_ERROR_CHECK(
        esp_timer_create(&poll_sensors_timer_args, &poll_sensors_timer));

    ESP_LOGI(TAG,
             "Starting timer to poll sensors every %u ms",
             sc_poll_sensors_period_ms);

    ESP_ERROR_CHECK(esp_timer_start_periodic(
        poll_sensors_timer, sc_poll_sensors_period_ms * 1000ULL));

    ui_run(*reinterpret_cast<i2c_master_bus_handle_t *>(
        s_poll_sensors_arg.i2c_bus->GetBusHandle()));

    ESP_LOGI(TAG, "Ending app_main");
}
