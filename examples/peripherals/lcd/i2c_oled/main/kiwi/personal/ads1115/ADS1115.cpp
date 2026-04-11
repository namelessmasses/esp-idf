#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ADS1115.hpp"

#include "../../I2C.hpp"
#include "../../II2C.hpp"

extern "C" {
#include "./ads1115.h"
}

#include <esp_err.h>
#include <esp_log.h>

#include <format>
#include <memory>
#include <stdexcept>

namespace kiwi::i2c {

namespace {
static const char *const TAG = "kiwi::i2c::ADS1115";
}

class ADS1115::Impl {
  public:
    Impl(uint8_t address, uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio)
        : m_pII2C(std::make_shared<I2C>(bus, sda_gpio, scl_gpio))
        , m_Address(address) {

        ESP_LOGD(TAG,
                 "Initializing ADS1115 at address 0x%02X on I2C bus %d",
                 m_Address,
                 m_pII2C->GetBusNumber());

        ESP_ERROR_CHECK(m_pII2C->AddBusDevice(m_Address));
    }

    Impl(std::shared_ptr<II2C> i2c, uint8_t address)
        : m_pII2C(i2c)
        , m_Address(address) {

        if (!i2c) {
            throw std::invalid_argument("I2C interface cannot be null");
        }

        ESP_LOGD(TAG,
                 "Initializing ADS1115 at address 0x%02X on I2C bus %d",
                 m_Address,
                 m_pII2C->GetBusNumber());

        ESP_ERROR_CHECK(m_pII2C->AddBusDevice(m_Address));
    }

    ~Impl() {
    }

    using internal_pga_type = ads1115_config_pga_t;

    std::shared_ptr<II2C> m_pII2C;
    uint8_t               m_Address;
    PGA                   m_PGA;
    float                 m_VoltageDividerScale = 1.f;

    static inline internal_pga_type GetPGAScale(PGA pga) {
        switch (pga) {
        case PGA::FS_6_144V:
            return ads1115_config_PGA_6_144V;
        case PGA::FS_4_096V:
            return ads1115_config_PGA_4_096V;
        case PGA::FS_2_048V:
            return ads1115_config_PGA_2_048V;
        case PGA::FS_1_024V:
            return ads1115_config_PGA_1_024V;
        case PGA::FS_0_512V:
            return ads1115_config_PGA_0_512V;
        case PGA::FS_0_256V:
            return ads1115_config_PGA_0_256V;
        default:
            throw std::invalid_argument(
                std::format("Invalid PGA value: {}", static_cast<int>(pga)));
        }
    }

    static float GetVoltageScale(PGA pga) {
        try {

            internal_pga_type gain = GetPGAScale(pga);
            return ads1115_gain_values[gain] / 32768.f;

        } catch (const std::exception &e) {

            // Handle invalid PGA value, possibly by logging the error and
            // returning a default scale
            ESP_LOGE(TAG, "Error getting voltage scale: %s", e.what());
            return 1.f; // Default scale if PGA value is invalid

        } catch (...) {

            // Handle any other unexpected exceptions
            ESP_LOGE(TAG, "Unknown error getting voltage scale");
            return 1.f; // Default scale in case of unknown error
        }
    }

    static uint32_t GetConversionDelayMs(ads1115_config_dr_t dr) {
        switch (dr) {
        case ads1115_config_DR_8SPS:
            return 125;
        case ads1115_config_DR_16SPS:
            return 63;
        case ads1115_config_DR_32SPS:
            return 32;
        case ads1115_config_DR_64SPS:
            return 16;
        case ads1115_config_DR_128SPS:
            return 8;
        case ads1115_config_DR_250SPS:
            return 4;
        case ads1115_config_DR_475SPS:
            return 2;
        case ads1115_config_DR_860SPS:
            return 2; // Datasheet specifies 1.2ms, but using 2ms to be safe
        default:
            ESP_LOGW(TAG, "Unknown data rate setting: %u", dr);
            return 8; // Default to 8ms for unknown data rate
        }
    }
};

ADS1115::ADS1115(uint8_t address,
                 uint8_t bus,
                 uint8_t sda_gpio,
                 uint8_t scl_gpio,
                 PGA     pga,
                 MUX     mux)
    : m_pImpl(std::make_unique<Impl>(address, bus, sda_gpio, scl_gpio)) {
    SetPGA(pga);
    SetMUX(mux);
}

ADS1115::ADS1115(std::shared_ptr<II2C> i2c, uint8_t address, PGA pga, MUX mux)
    : m_pImpl(std::make_unique<Impl>(i2c, address)) {
    SetPGA(pga);
    SetMUX(mux);
}

void ADS1115::SetPGA(PGA pga) {
    m_pImpl->m_PGA = pga;

    // Read the current config register value to preserve other settings like
    // MUX, MODE, etc.
    ads1115_register_t config_reg = {
        .address = {.P = ADS1115_REG_CONFIG, .RESERVED = 0},
        .reg     = {.raw = 0},
    };
    m_pImpl->m_pII2C->Read(m_pImpl->m_Address,
                           reinterpret_cast<uint8_t *>(&config_reg.reg.raw),
                           sizeof(config_reg.reg.raw),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

    // Update the PGA bits in the config register
    config_reg.reg.config.PGA = Impl::GetPGAScale(pga);

    // Write the updated config register back to the device
    m_pImpl->m_pII2C->Write(m_pImpl->m_Address,
                            reinterpret_cast<const uint8_t *>(&config_reg),
                            sizeof(config_reg),
                            ADS1115::k_DEFAULT_TIMEOUT_MS);
}

void ADS1115::SetMUX(MUX mux) {
    // Read the current config register value to preserve other settings like
    // PGA, MODE, etc.
    ads1115_register_t config_reg = {
        .address = {.P = ADS1115_REG_CONFIG, .RESERVED = 0},
        .reg     = {.raw = 0},
    };
    m_pImpl->m_pII2C->Read(m_pImpl->m_Address,
                           reinterpret_cast<uint8_t *>(&config_reg.reg.raw),
                           sizeof(config_reg.reg.raw),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

    // Update the MUX bits in the config register
    config_reg.reg.config.MUX = static_cast<uint16_t>(mux);

    // Write the updated config register back to the device
    m_pImpl->m_pII2C->Write(m_pImpl->m_Address,
                            reinterpret_cast<const uint8_t *>(&config_reg),
                            sizeof(config_reg),
                            ADS1115::k_DEFAULT_TIMEOUT_MS);
}

void ADS1115::SetVoltageDivider(float r1, float r2) {
    if (r2 <= 0.f) {
        throw std::invalid_argument(
            std::format("Invalid resistor values: R1={}, R2={}", r1, r2));
    }

    m_pImpl->m_VoltageDividerScale = (r1 + r2) / r2;
}

float ADS1115::GetVoltage() {

    // Read config register
    ads1115_register_t reg = {
        .address = {.P = ADS1115_REG_CONFIG, .RESERVED = 0},
        .reg     = {.raw = 0},
    };
    m_pImpl->m_pII2C->Read(m_pImpl->m_Address,
                           reinterpret_cast<uint8_t *>(&reg.reg.raw),
                           sizeof(reg.reg.raw),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

    // Start single conversion by setting the OS bit to 1
    reg.reg.config.OS = 1;
    m_pImpl->m_pII2C->Write(m_pImpl->m_Address,
                            reinterpret_cast<const uint8_t *>(&reg),
                            sizeof(reg),
                            ADS1115::k_DEFAULT_TIMEOUT_MS);

    // Use the DR (data rate) bits from the config register to determine the
    // appropriate delay
    const ads1115_config_dr_t dr =
        static_cast<ads1115_config_dr_t>(reg.reg.config.DR);

    // Calculate the delay based on the data rate (DR) setting
    const TickType_t delay_ticks =
        pdMS_TO_TICKS(Impl::GetConversionDelayMs(dr)) + 1;

    bool isBusy = true;
    while (isBusy) {
        vTaskDelay(delay_ticks);
        m_pImpl->m_pII2C->Read(m_pImpl->m_Address,
                               reinterpret_cast<uint8_t *>(&reg.reg.raw),
                               sizeof(reg.reg.raw),
                               ADS1115::k_DEFAULT_TIMEOUT_MS);
        isBusy = (reg.reg.config.OS == 1);
    }

    reg.address.P        = ADS1115_REG_CONVERSION;
    uint16_t raw_reading = 0;
    m_pImpl->m_pII2C->Read(m_pImpl->m_Address,
                           reinterpret_cast<uint8_t *>(&raw_reading),
                           sizeof(raw_reading),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

    float volategScale = Impl::GetVoltageScale(m_pImpl->m_PGA);
    float voltage      = volategScale * raw_reading;
    voltage *= m_pImpl->m_VoltageDividerScale;

    return voltage;
}

ADS1115::~ADS1115() {
}

} // namespace kiwi::i2c