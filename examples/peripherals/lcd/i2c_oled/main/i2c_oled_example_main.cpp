/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "./kiwi/I2CFactory.hpp"
#include "./kiwi/II2C.hpp"
#include "./kiwi/personal/ads1115/ADS1115.hpp"
#include "./kiwi/personal/sht41/SHT41.hpp"

extern "C" {

#include "watchdog_util.h"

#include <freertos/FreeRTOS.h>

#include <freertos/task.h>

#include <driver/i2c_types.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_log_level.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>

#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>
}

#include <cmath>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "sensor_data.h"

static const char *TAG = "tote_monitor";

static constexpr uint32_t k_TOTE_MONITOR_I2C_BUS_PORT = 0;
static constexpr uint32_t k_TOTE_MONITOR_PIN_NUM_SDA  = 21;
static constexpr uint32_t k_TOTE_MONITOR_PIN_NUM_SCL  = 22;
static constexpr uint32_t k_TOTE_MONITOR_PIN_NUM_RST  = -1;

static constexpr uint32_t k_POLL_SENSORS_PERIOD_MS = 500;

typedef struct {
    std::shared_ptr<kiwi::i2c::II2C>    i2c_bus;
    std::unique_ptr<kiwi::i2c::ADS1115> p_ADS1115;
    std::unique_ptr<kiwi::i2c::SHT41>   p_SHT41;
} poll_sensors_arg_t;

static constexpr float k_VOLTAGE_DIVIDER_RTOP    = 0.f; // 99.6e3f;
static constexpr float k_VOLTAGE_DIVIDER_RBOTTOM = 1.f; // 9.91e3f;
static constexpr float k_VOLTAGE_DIVIDER_REVERSE_MULTIPLIER =
    (k_VOLTAGE_DIVIDER_RTOP + k_VOLTAGE_DIVIDER_RBOTTOM) /
    k_VOLTAGE_DIVIDER_RBOTTOM;

static poll_sensors_arg_t s_poll_sensors_arg = {
    .i2c_bus = nullptr, .p_ADS1115 = nullptr, .p_SHT41 = nullptr};

static void poll_sensors(void *arg) {
    try {
        poll_sensors_arg_t *poll_arg = (poll_sensors_arg_t *)arg;

        int32_t data_index = g_sensor_data.index.load();
        ++data_index;
        data_index = data_index & 1; // toggle between 0 and 1

        g_sensor_data.data[data_index].voltage    = NAN;
        g_sensor_data.data[data_index].temp_humid = {NAN, NAN, NAN};

        float voltage = g_sensor_data.data[data_index].voltage =
            poll_arg->p_ADS1115->GetVoltage() *
            k_VOLTAGE_DIVIDER_REVERSE_MULTIPLIER;
        if (std::isnan(voltage)) {
            ESP_LOGW(TAG,
                     "Failed to read from ADS1115 sensor - voltage is NaN");
        } else {
            g_sensor_data.data[data_index].voltage = voltage;
            ESP_LOGI(TAG, "Voltage reading updated: voltage=%.2f V", voltage);
        }

        kiwi::i2c::SHT41::Reading reading = poll_arg->p_SHT41->GetReading();
        if (std::isnan(reading.relative_humidity)) {
            ESP_LOGW(
                TAG,
                "Failed to read from SHT41 sensor - relative humidity is NaN");
        } else if (std::isnan(reading.temperature_celcius)) {
            ESP_LOGW(TAG,
                     "Failed to read from SHT41 sensor - temperature is NaN");
        } else if (std::isnan(reading.temperature_fahrenheit)) {
            ESP_LOGW(TAG,
                     "Failed to read from SHT41 sensor - temperature is NaN");
        } else {
            g_sensor_data.data[data_index].temp_humid = reading;
        }

        g_sensor_data.index.store(data_index);

        ESP_LOGI(
            TAG,
            "Sensor readings updated: voltage=%.2f V; temperature=%.2f C; "
            "temperature=%.2f F; humidity=%.2f %%",
            g_sensor_data.data[data_index].voltage,
            g_sensor_data.data[data_index].temp_humid.temperature_celcius,
            g_sensor_data.data[data_index].temp_humid.temperature_fahrenheit,
            g_sensor_data.data[data_index].temp_humid.relative_humidity);
    } catch (const std::exception &e) {
        ESP_LOGE(
            TAG, "%s: Exception while polling sensors: %s", __func__, e.what());
    } catch (...) {
        ESP_LOGE(TAG, "%s: Unknown exception while polling sensors", __func__);
    }
}

extern "C" void ui_run(i2c_master_bus_handle_t bus_handle);

extern "C" void app_main(void) {

    wait_for_gdb_attach();

    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Initializing I2C bus");
    s_poll_sensors_arg.i2c_bus =
        kiwi::i2c::factory::CreateI2C(kiwi::i2c::factory::I2CDriver::New,
                                      k_TOTE_MONITOR_I2C_BUS_PORT,
                                      k_TOTE_MONITOR_PIN_NUM_SDA,
                                      k_TOTE_MONITOR_PIN_NUM_SCL);

    if (s_poll_sensors_arg.i2c_bus == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return;
    }

    yield_for_watchdog();

    try {
        ESP_LOGI(TAG, "Adding ADS1115 to the I2C bus");
        kiwi::i2c::ADS1115::s_LogLevel = ESP_LOG_INFO;
        s_poll_sensors_arg.p_ADS1115   = std::make_unique<kiwi::i2c::ADS1115>(
            s_poll_sensors_arg.i2c_bus,
            kiwi::i2c::ADS1115::k_DEFAULT_I2C_ADDRESS,
            kiwi::i2c::ADS1115::PGA::DEFAULT,
            kiwi::i2c::ADS1115::MUX::AIN0_GND);

        if (s_poll_sensors_arg.p_ADS1115 == nullptr) {
            ESP_LOGE(TAG, "Failed to initialize ADS1115 sensor");
            return;
        }

        ESP_LOGI(TAG, "Adding SHT41 to the I2C bus");
        s_poll_sensors_arg.p_SHT41 =
            std::make_unique<kiwi::i2c::SHT41>(s_poll_sensors_arg.i2c_bus);

        if (s_poll_sensors_arg.p_SHT41 == nullptr) {
            ESP_LOGE(TAG, "Failed to initialize SHT41 sensor");
            return;
        }

    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Exception while initializing sensors: %s", e.what());
        return;
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception while initializing sensors");
        return;
    }

    yield_for_watchdog();

    ESP_LOGI(TAG,
             "Creating timer to poll sensors every %u ms",
             k_POLL_SENSORS_PERIOD_MS);
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
             k_POLL_SENSORS_PERIOD_MS);

    ESP_ERROR_CHECK(esp_timer_start_periodic(
        poll_sensors_timer, k_POLL_SENSORS_PERIOD_MS * 1000ULL));

    void *pv_bus_handle = s_poll_sensors_arg.i2c_bus->GetBusHandle();

    i2c_master_bus_handle_t bus_handle =
        *reinterpret_cast<i2c_master_bus_handle_t *>(&pv_bus_handle);

    yield_for_watchdog();

    ui_run(bus_handle);

    ESP_LOGI(TAG, "Sleeping indefinitely");
    while (true) {
        yield_for_watchdog();
    }
}
