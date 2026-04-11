#include "SHT41.hpp"

#include "../../II2C.hpp"

extern "C" {
#include "sht41.h"
}

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <limits>
#include <memory>

namespace {
static const char *TAG = "kiwi::SHT41";

static void process_sht41_data(const uint8_t *raw_data, sht41_data_t *data) {

    data->temp_ticks = raw_data[0] << 8 | raw_data[1];
    data->temperature_celcius =
        -45.f + 175.f * ((float)data->temp_ticks / 65535.f);
    data->temperature_fahrenheit =
        -49.f + 315.f * ((float)data->temp_ticks / 65535.f);

    data->rh_ticks          = raw_data[3] << 8 | raw_data[4];
    data->relative_humidity = -6.f + 125.f * ((float)data->rh_ticks / 65535.f);
}

} // namespace

namespace kiwi::i2c {

class SHT41::Impl {

  public:
    Impl() = delete;

    ~Impl() {
    }

    Impl(std::shared_ptr<II2C> const &i2c, uint8_t address)
        : m_pII2C(i2c)
        , m_address(address) {
        m_pII2C->AddBusDevice(m_address);
    }

    std::shared_ptr<II2C> m_pII2C;
    uint8_t               m_address;

    static constexpr uint32_t k_I2C_MASTER_TIMEOUT_MS       = 1000;
    static constexpr uint32_t k_SHT41_READ_WAIT_DURATION_MS = 1000;
};

SHT41::SHT41(std::shared_ptr<II2C> const &i2c, uint8_t address)
    : pImpl(std::make_unique<Impl>(i2c, address)) {
}

SHT41::Reading SHT41::GetReading() {

    const uint8_t cmd = CMD_READ_LOW_PRECISION;
    esp_err_t     err = pImpl->m_pII2C->Write(
        pImpl->m_address, &cmd, sizeof(cmd), Impl::k_I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to send read command to SHT41 sensor: %s",
                 esp_err_to_name(err));
        return {std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN()};
    }

    vTaskDelay(pdMS_TO_TICKS(Impl::k_SHT41_READ_WAIT_DURATION_MS));

    uint8_t   raw_data[6] = {0};
    esp_err_t res         = ESP_ERR_TIMEOUT;
    for (int i = 0; i < 3; i++) {
        res = pImpl->m_pII2C->Read(pImpl->m_address,
                                   raw_data,
                                   sizeof(raw_data),
                                   Impl::k_I2C_MASTER_TIMEOUT_MS);
        if (res == ESP_OK) {
            break;
        }

        if (res == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG,
                     "I2C bus is busy, retrying in %ums...",
                     Impl::k_SHT41_READ_WAIT_DURATION_MS);
        } else {
            ESP_LOGW(TAG,
                     "Error reading from sensor: %s, retrying in %ums...",
                     esp_err_to_name(res),
                     Impl::k_SHT41_READ_WAIT_DURATION_MS);
        }

        vTaskDelay(pdMS_TO_TICKS(Impl::k_SHT41_READ_WAIT_DURATION_MS));
    }

    if (res != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to read data from sensor after multiple attempts");
        return {std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN()};
    }

    sht41_data_t data;
    process_sht41_data(raw_data, &data);

    return {data.temperature_celcius,
            data.temperature_fahrenheit,
            data.relative_humidity};
}

SHT41::~SHT41() {}

} // namespace kiwi