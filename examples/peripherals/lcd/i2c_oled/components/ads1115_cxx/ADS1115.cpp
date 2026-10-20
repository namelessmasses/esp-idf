#include "./ADS1115.hpp"

#include "ads1115_i2c_codec.h"
#include "esp_log_config.h"
#include "kiwi2c/II2C.hpp"

extern "C" {

#include "ads1115.h"

#include "./endian.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "portmacro.h"

#include <esp_cpu.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_log_level.h>
}

#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <stdexcept>

bool operator==(const ads1115_config_register_t &lhs,
                const ads1115_config_register_t &rhs) {
    return *reinterpret_cast<const uint16_t *>(&lhs) ==
           *reinterpret_cast<const uint16_t *>(&rhs);
}

namespace kiwi::i2c {

namespace {
static const char *const TAG = "kiwi::i2c::ADS1115";
}

esp_log_level_t ADS1115::s_LogLevel = ESP_LOG_DEBUG;

class ADS1115::Impl {
  public:
    Impl(std::shared_ptr<II2C> i2c, uint8_t address)
        : m_pII2C(i2c)
        , m_Address(address)
        , m_ConfigReg{}
        , m_ConversionDelayTicks(0) {

        if (!i2c) {
            throw std::invalid_argument("I2C interface cannot be null");
        }

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Initializing ADS1115 at address 0x%02X",
                      m_Address);

        ESP_ERROR_CHECK(m_pII2C->AddBusDevice(m_Address));

        ESP_LOG_LEVEL_LOCAL(ADS1115::s_LogLevel,
                            TAG,
                            "Reading initial configuration register from "
                            "device at address 0x%02X",
                            m_Address);

        ReadRegister(m_ConfigReg);

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Initial configuration register retrieved from device at "
                      "address 0x%02X",
                      m_Address);

        // Set data rate to 8SPS since we don't need fast conversions, and this
        // allows for more stable readings with the ADS1115's internal PGA
        // enabled.
        //
        // Set comparator queue to disabled, since we won't be using the
        // comparator.
        m_ConfigReg.DR       = ads1115_config_DR_8SPS;
        m_ConfigReg.COMP_QUE = ads1115_config_COMP_QUE_DISABLE;

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Writing updated configuration register to ADS1115 at "
                      "address 0x%02X",
                      m_Address);

        WriteRegister(m_ConfigReg);

        ESP_LOGD(TAG, "Reading back config register to verify");

        ads1115_config_register_t config_reg_check = {.only_for_simple_init =
                                                          0};

        // Wait 100ms for the update?
        vTaskDelay(pdMS_TO_TICKS(100));

        ReadRegister(config_reg_check);
        if (config_reg_check != m_ConfigReg) {
            throw std::runtime_error(
                std::format("{}: config register verification failed: "
                            "expected {}, "
                            "read back {}",
                            TAG,
                            m_ConfigReg,
                            config_reg_check));
        }

        // Set the conversion delay based on the data rate (DR) bits in the
        // config register
        const ads1115_config_dr_t dr =
            static_cast<ads1115_config_dr_t>(m_ConfigReg.DR);
        const uint32_t c_samples_per_second  = GetSamplesPerSecond(dr);
        const uint32_t c_conversion_delay_ms = GetConversionDelayMs(dr);
        m_ConversionDelayTicks = pdMS_TO_TICKS(c_conversion_delay_ms) + 1;
        ESP_LOG_LEVEL(
            ADS1115::s_LogLevel,
            TAG,
            "ADS1115 data rate setting: %u SPS; conversion delay: %u ms",
            c_samples_per_second,
            c_conversion_delay_ms);
    }

    ~Impl() {
    }

    std::shared_ptr<II2C>     m_pII2C;
    uint8_t                   m_Address;
    ads1115_config_register_t m_ConfigReg;
    TickType_t                m_ConversionDelayTicks;
    float                     m_ConversionPGAMultiplier;

    /**
     * \brief Overload for writing to the conversion register (not allowed)
     * \param reg The conversion register to write to (not used)
     * \throws std::invalid_argument Always throws since writing to the
     * conversion register is not allowed.
     */
    void WriteRegister(ads1115_conversion_register_t reg) {
        // The conversion register is read-only, so writing to it doesn't make
        // sense.
        throw std::invalid_argument(
            "Cannot write to conversion register - it is read-only");
    }

    /**
     * \brief Overload to write to the config register
     * \param reg The config register to write.
     * \throws std::runtime_error If the I2C write fails.
     *
     * \details Packs a I2CWireFormatConfig structure using shift-and-OR with
     * the appropriate register address and config register bits, then writes it
     * over I2C.
     */
    void WriteRegister(ads1115_config_register_t reg) {

        ESP_LOG_LEVEL_LOCAL(
            ADS1115::s_LogLevel, TAG, "Writing Config Register:");
        LogRegister(ADS1115::s_LogLevel, reg);

        uint8_t wire_bytes[3] = {0};

        wire_bytes[0] = ADS1115_REG_CONFIG;
        wire_bytes[1] = static_cast<uint8_t>((reg.OS << 7) | (reg.MUX << 4) |
                                             (reg.PGA << 1) | (reg.MODE));
        wire_bytes[2] = static_cast<uint8_t>(
            (reg.DR << 5) | (reg.COMP_MODE << 4) | (reg.COMP_POL << 3) |
            (reg.COMP_LAT << 1) | (reg.COMP_QUE));

        ESP_LOGD(TAG,
                 "Config register wire bytes: 0x%02x%02x",
                 wire_bytes[1],
                 wire_bytes[2]);

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(wire_bytes),
                           sizeof(wire_bytes),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (write_err != ESP_OK) {
            throw std::runtime_error(
                std::format("{}: Failed to write config register: {}",
                            TAG,
                            esp_err_to_name(write_err)));
        }

        m_ConfigReg = reg;
    }

    /**
     * \brief Overload for writing the LO_THRESH register
     */
    void WriteRegister(ads1115_lo_thresh_register_t reg) {

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Writing LO_THRESH register with value: %hu",
                      reg.value);

        uint8_t wire_bytes[3] = {0};

        wire_bytes[0] = ADS1115_REG_LO_THRESH;
        wire_bytes[1] = static_cast<uint8_t>((reg.value >> 8) & 0xFF);
        wire_bytes[2] = static_cast<uint8_t>(reg.value & 0xFF);

        ESP_LOGD(TAG,
                 "LO_THRESH register wire bytes: 0x%02x%02x",
                 wire_bytes[1],
                 wire_bytes[2]);

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(wire_bytes),
                           sizeof(wire_bytes),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (write_err != ESP_OK) {
            throw std::runtime_error(
                std::format("{}: Failed to write LO_THRESH register: {}",
                            TAG,
                            esp_err_to_name(write_err)));
        }

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Wrote LO_THRESH register with value: 0x%02x%02x",
                      wire_bytes[1],
                      wire_bytes[2]);
    }

    /**
     * \brief Overload for writing the HI_THRESH register
     */
    void WriteRegister(ads1115_hi_thresh_register_t reg) {

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Writing HI_THRESH register with value: %hu",
                      reg.value);

        uint8_t wire_bytes[3] = {0};

        wire_bytes[0] = ADS1115_REG_HI_THRESH;
        wire_bytes[1] = static_cast<uint8_t>((reg.value >> 8) & 0xFF);
        wire_bytes[2] = static_cast<uint8_t>(reg.value & 0xFF);

        ESP_LOGD(TAG,
                 "HI_THRESH register wire bytes: 0x%02x%02x",
                 wire_bytes[1],
                 wire_bytes[2]);

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(wire_bytes),
                           sizeof(wire_bytes),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (write_err != ESP_OK) {
            throw std::runtime_error(
                std::format("{}: Failed to write HI_THRESH register: {}",
                            TAG,
                            esp_err_to_name(write_err)));
        }

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Wrote HI_THRESH register with value: 0x%02x%02x",
                      wire_bytes[1],
                      wire_bytes[2]);
    }

#if 0
    void WriteRegister(ads1115_register_t r) {
        switch (r.address.P) {
        case ADS1115_REG_CONVERSION:
            WriteRegister(r.conversion);
            break;
        case ADS1115_REG_CONFIG:
            WriteRegister(r.config);
            break;
        case ADS1115_REG_LO_THRESH:
            WriteRegister(r.lo_thresh);
            break;
        case ADS1115_REG_HI_THRESH:
            WriteRegister(r.hi_thresh);
            break;
        default:
            throw std::invalid_argument(std::format(
                "Invalid register address for write: 0x{:02x}", r.address.P));
        }
    }
#endif

    /**
     * \brief Overload for reading the conversion register
     */
    ads1115_register_t
    ReadRegister(ads1115_conversion_register_t &conversion_reg) {

        const ads1115_address_pointer_register_t address_pointer_register = {
            .P = ADS1115_REG_CONVERSION};

        uint8_t wire_bytes[2] = {0};

        const esp_err_t read_request_err = m_pII2C->WriteRead(
            m_Address,
            reinterpret_cast<const uint8_t *>(&address_pointer_register),
            sizeof(ads1115_address_pointer_register_t),
            reinterpret_cast<uint8_t *>(wire_bytes),
            sizeof(wire_bytes),
            ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (read_request_err != ESP_OK) {
            throw std::runtime_error(
                std::format("Failed to read conversion register: {}",
                            esp_err_to_name(read_request_err)));
        }

        ESP_LOGD(TAG,
                 "Read conversion register: 0x%02x%02x",
                 wire_bytes[0],
                 wire_bytes[1]);

        // Unpack the wire bits
        conversion_reg.conversion_result = (wire_bytes[0] << 8) | wire_bytes[1];

        ads1115_register_t reg_to_return = {
            .address    = {.P = ADS1115_REG_CONVERSION},
            .conversion = conversion_reg};

        LogRegister(ADS1115::s_LogLevel, reg_to_return.conversion);

        return reg_to_return;
    }

    /**
     * \brief Overload for reading the config register
     */
    ads1115_register_t ReadRegister(ads1115_config_register_t &config_reg) {

        const ads1115_address_pointer_register_t address_pointer_register = {
            .P = ADS1115_REG_CONFIG};

        uint8_t wire_bytes[2] = {0};

        const esp_err_t read_request_err = m_pII2C->WriteRead(
            m_Address,
            reinterpret_cast<const uint8_t *>(&address_pointer_register),
            sizeof(ads1115_address_pointer_register_t),
            reinterpret_cast<uint8_t *>(wire_bytes),
            sizeof(wire_bytes),
            ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (read_request_err != ESP_OK) {
            throw std::runtime_error(
                std::format("Failed to read config register: {}",
                            esp_err_to_name(read_request_err)));
        }

        ESP_LOGD(TAG,
                 "Read config register: 0x%02x%02x",
                 wire_bytes[1],
                 wire_bytes[0]);

        // Unpack the wire bits
        config_reg.OS        = (wire_bytes[1] >> 7) & 0x1;
        config_reg.MUX       = (wire_bytes[1] >> 4) & 0x3;
        config_reg.PGA       = (wire_bytes[1] >> 1) & 0x3;
        config_reg.MODE      = (wire_bytes[1]) & 0x1;
        config_reg.DR        = (wire_bytes[0] >> 5) & 0x7;
        config_reg.COMP_MODE = (wire_bytes[0] >> 4) & 0x1;
        config_reg.COMP_POL  = (wire_bytes[0] >> 3) & 0x1;
        config_reg.COMP_LAT  = (wire_bytes[0] >> 2) & 0x1;
        config_reg.COMP_QUE  = wire_bytes[0] & 0x3;

        ads1115_register_t reg_to_return = {
            .address = {.P = ADS1115_REG_CONFIG}, .config = config_reg};

        ESP_LOG_LEVEL_LOCAL(
            ADS1115::s_LogLevel, TAG, "Retrieved Config Register:");
        LogRegister(ADS1115::s_LogLevel, reg_to_return.config);

        return reg_to_return;
    }

    /**
     * \brief Overload for reading the LO_THRESH register
     */
    ads1115_register_t
    ReadRegister(ads1115_lo_thresh_register_t &lo_thresh_reg) {

        const ads1115_address_pointer_register_t address_pointer_register = {
            .P = ADS1115_REG_LO_THRESH};

        uint8_t wire_bytes[2] = {0};

        const esp_err_t read_request_err = m_pII2C->WriteRead(
            m_Address,
            reinterpret_cast<const uint8_t *>(&address_pointer_register),
            sizeof(ads1115_address_pointer_register_t),
            reinterpret_cast<uint8_t *>(wire_bytes),
            sizeof(wire_bytes),
            ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (read_request_err != ESP_OK) {
            throw std::runtime_error(
                std::format("Failed to read LO_THRESH register: {}",
                            esp_err_to_name(read_request_err)));
        }

        ESP_LOGD(TAG,
                 "Read LO_THRESH register: 0x%02x%02x",
                 wire_bytes[0],
                 wire_bytes[1]);

        // Unpack the wire bits
        lo_thresh_reg.value = (wire_bytes[0] << 8) | wire_bytes[1];

        ads1115_register_t reg_to_return = {
            .address   = {.P = ADS1115_REG_LO_THRESH},
            .lo_thresh = lo_thresh_reg};

        LogRegister(ADS1115::s_LogLevel, reg_to_return.lo_thresh);

        return reg_to_return;
    }

    /**
     * \brief Overload for reading the HI_THRESH register
     */
    ads1115_register_t
    ReadRegister(ads1115_hi_thresh_register_t &hi_thresh_reg) {

        const ads1115_address_pointer_register_t address_pointer_register = {
            .P = ADS1115_REG_HI_THRESH};

        uint8_t wire_bytes[2] = {0};

        const esp_err_t read_request_err = m_pII2C->WriteRead(
            m_Address,
            reinterpret_cast<const uint8_t *>(&address_pointer_register),
            sizeof(ads1115_address_pointer_register_t),
            reinterpret_cast<uint8_t *>(wire_bytes),
            sizeof(wire_bytes),
            ADS1115::k_DEFAULT_TIMEOUT_MS);

        if (read_request_err != ESP_OK) {
            throw std::runtime_error(
                std::format("Failed to read HI_THRESH register: {}",
                            esp_err_to_name(read_request_err)));
        }

        ESP_LOGD(TAG,
                 "Read HI_THRESH register: 0x%04X",
                 (wire_bytes[0] << 8) | wire_bytes[1]);

        // Unpack the wire bits
        hi_thresh_reg.value = (wire_bytes[0] << 8) | wire_bytes[1];

        ads1115_register_t reg_to_return = {
            .address   = {.P = ADS1115_REG_HI_THRESH},
            .hi_thresh = hi_thresh_reg};

        LogRegister(ADS1115::s_LogLevel, reg_to_return.hi_thresh);

        return reg_to_return;
    }

    void LogRegister(esp_log_level_t                  log_level,
                     const ads1115_config_register_t &config_reg) {

        ESP_LOG_LEVEL_LOCAL(log_level,
                            TAG,
                            "Config Register: OS[0] %u, MUX[2:0] %u, PGA[2:0] "
                            "%u, MODE[0] %u, DR[2:0] %u, COMP_MODE: %u, "
                            "COMP_POL: %u, COMP_LAT: %u, COMP_QUE[1:0] %u",
                            config_reg.OS,
                            config_reg.MUX,
                            config_reg.PGA,
                            config_reg.MODE,
                            config_reg.DR,
                            config_reg.COMP_MODE,
                            config_reg.COMP_POL,
                            config_reg.COMP_LAT,
                            config_reg.COMP_QUE);
    }

    void LogRegister(esp_log_level_t                      log_level,
                     const ads1115_conversion_register_t &conversion_reg) {

        ESP_LOG_LEVEL_LOCAL(log_level,
                            TAG,
                            "Conversion Register: 0x%04x %hu",
                            conversion_reg.conversion_result,
                            conversion_reg.conversion_result);
    }

    void LogRegister(esp_log_level_t                     log_level,
                     const ads1115_hi_thresh_register_t &hi_thresh_reg) {

        ESP_LOG_LEVEL_LOCAL(log_level,
                            TAG,
                            "HI_THRESH Register: 0x%04x %hu",
                            hi_thresh_reg.value,
                            hi_thresh_reg.value);
    }

    void LogRegister(esp_log_level_t                     log_level,
                     const ads1115_lo_thresh_register_t &lo_thresh_reg) {

        ESP_LOG_LEVEL_LOCAL(log_level,
                            TAG,
                            "LO_THRESH Register: 0x%04x %hu",
                            lo_thresh_reg.value,
                            lo_thresh_reg.value);
    }

    /**
     * \brief Set the conversion PGA multiplier based on the given PGA setting
     * \param pga The PGA setting to use for determining the conversion
     * multiplier
     */
    void SetConversionPGAMultiplier(ads1115_config_pga_t pga) {

        m_ConversionPGAMultiplier =
            ads1115_gain_values[static_cast<uint16_t>(pga)];

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Set PGA scale to %.6f V/LSB based on PGA setting %u",
                      m_ConversionPGAMultiplier,
                      pga);
    }

    void SetPGA(ads1115_config_pga_t pga) {

        ads1115_config_register_t new_config_reg = m_ConfigReg;

        // Update the PGA bits in the config register
        new_config_reg.PGA = pga;

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Setting ADS1115 PGA to %u; writing config register",
                      pga);

        WriteRegister(new_config_reg);

        // Update the cached PGA scale
        SetConversionPGAMultiplier(pga);
    }

    void SetPGA(PGA pga) {
        SetPGA(Impl::GetPGA(pga));
    }

    static inline ads1115_config_pga_t GetPGA(PGA pga) {
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

    static uint32_t GetSamplesPerSecond(ads1115_config_dr_t dr) {
        uint32_t sps = 0;
        switch (dr) {
        case ads1115_config_DR_8SPS:
            sps = 8;
            break;
        case ads1115_config_DR_16SPS:
            sps = 16;
            break;
        case ads1115_config_DR_32SPS:
            sps = 32;
            break;
        case ads1115_config_DR_64SPS:
            sps = 64;
            break;
        case ads1115_config_DR_128SPS:
            sps = 128;
            break;
        case ads1115_config_DR_250SPS:
            sps = 250;
            break;
        case ads1115_config_DR_475SPS:
            sps = 475;
            break;
        case ads1115_config_DR_860SPS:
            sps = 860;
            break;
        default:
            ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                          TAG,
                          "Unknown data rate setting in config register: %u",
                          dr);

            throw std::runtime_error(std::format(
                "Unknown data rate setting in config register: 0x{:04x}",
                static_cast<uint16_t>(dr)));
        }
        return sps;
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
            ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                          TAG,
                          "Unknown data rate setting in config register: %u",
                          dr);

            throw std::runtime_error(std::format(
                "Unknown data rate setting in config register: 0x{:04x}",
                static_cast<uint16_t>(dr)));
        }
    }

    static ads1115_config_mux_t GetMUX(MUX mux) {
        switch (mux) {
        case MUX::AIN0_GND:
            return ads1115_config_MUX_AIN0_GND;
        case MUX::AIN1_GND:
            return ads1115_config_MUX_AIN1_GND;
        case MUX::AIN2_GND:
            return ads1115_config_MUX_AIN2_GND;
        case MUX::AIN3_GND:
            return ads1115_config_MUX_AIN3_GND;
        case MUX::AIN0_AIN1:
            return ads1115_config_MUX_AIN0_AIN1;
        case MUX::AIN0_AIN3:
            return ads1115_config_MUX_AIN0_AIN3;
        case MUX::AIN1_AIN3:
            return ads1115_config_MUX_AIN1_AIN3;
        case MUX::AIN2_AIN3:
            return ads1115_config_MUX_AIN2_AIN3;
        default:
            throw std::invalid_argument(
                std::format("Invalid MUX value: {}", static_cast<int>(mux)));
        }
    }
};

ADS1115::ADS1115(std::shared_ptr<II2C> i2c, uint8_t address, PGA pga, MUX mux)
    : m_pImpl(std::make_unique<Impl>(i2c, address)) {
    SetPGA(pga);
    SetMUX(mux);
}

void ADS1115::SetPGA(PGA pga) {
    m_pImpl->SetPGA(pga);
}

void ADS1115::SetMUX(MUX mux) {
    ads1115_config_register_t config_reg = m_pImpl->m_ConfigReg;

    // Update the MUX bits in the config register
    config_reg.MUX = Impl::GetMUX(mux);

    ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                  TAG,
                  "Setting ADS1115 MUX to %u; writing config register",
                  config_reg.MUX);

    m_pImpl->WriteRegister(config_reg);
}

float ADS1115::GetVoltage() {

    try {
        ads1115_config_register_t reg = m_pImpl->m_ConfigReg;

        // Request Start single conversion by setting the OS bit to 1
        ESP_LOG_LEVEL(
            ADS1115::s_LogLevel,
            TAG,
            "Requesting starting single conversion by setting OS bit to "
            "1 in config register");
        reg.OS = ads1115_config_OS_WRITE_START_SINGLE_CONVERSION;
        m_pImpl->WriteRegister(reg);

        bool conversion_complete = false;
        for (uint32_t i = 0; i < 16; ++i) {
            ESP_LOG_LEVEL(
                ADS1115::s_LogLevel,
                TAG,
                "Waiting %u ms for conversion to complete (attempt %u/16)",
                pdTICKS_TO_MS(m_pImpl->m_ConversionDelayTicks),
                i + 1);
            vTaskDelay(m_pImpl->m_ConversionDelayTicks);

            ESP_LOG_LEVEL(
                ADS1115::s_LogLevel,
                TAG,
                "Polling ADS1115 config register to check if conversion is "
                "complete by checking OS bit");
            m_pImpl->ReadRegister(reg);

            conversion_complete =
                (reg.OS == ads1115_config_OS_READ_CONVERSION_NOT_IN_PROGRESS);
            if (conversion_complete) {
                break;
            }
        }

        if (!conversion_complete) {
            ESP_LOGE(TAG, "ADS1115 conversion timed out");
            return std::numeric_limits<float>::quiet_NaN();
        }

        ESP_LOG_LEVEL(
            ADS1115::s_LogLevel,
            TAG,
            "ADS1115 conversion complete; reading conversion register");
        ads1115_conversion_register_t conversion_reg;
        m_pImpl->ReadRegister(conversion_reg);

        float voltage = m_pImpl->m_ConversionPGAMultiplier *
                        conversion_reg.conversion_result /
                        32768.f; // 32768 = 2^15, since ADS1115 is a 16-bit
                                 // ADC with one bit for sign
        ESP_LOG_LEVEL(
            ADS1115::s_LogLevel,
            TAG,
            "Raw ADC reading from ADS1115: 0x%04X; signed value: %d; PGA "
            "scale: %.6f V/LSB, calculated voltage = %.6f V",
            conversion_reg.conversion_result,
            conversion_reg.conversion_result,
            m_pImpl->m_ConversionPGAMultiplier,
            voltage);

        return voltage;
    } catch (const std::exception &e) {
        ESP_LOGE(
            TAG, "Exception while getting voltage from ADS1115: %s", e.what());
    } catch (...) {
        ESP_LOGE(TAG, "Unknown exception while getting voltage from ADS1115");
    }

    return std::numeric_limits<float>::quiet_NaN();
}

ADS1115::~ADS1115() {
}

} // namespace kiwi::i2c