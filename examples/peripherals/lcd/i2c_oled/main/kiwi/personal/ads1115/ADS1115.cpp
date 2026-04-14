#include "./ADS1115.hpp"

#include "../../I2C.hpp"
#include "../../II2C.hpp"

extern "C" {

#include "./ads1115.h"

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
    return lhs.raw == rhs.raw;
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
                      "Initializing ADS1115 at address 0x%02X on I2C bus %d",
                      m_Address,
                      m_pII2C->GetBusNumber());

        ESP_ERROR_CHECK(m_pII2C->AddBusDevice(m_Address));

        ReadRegister(m_ConfigReg);

        // Set data rate to 8SPS since we don't need fast conversions, and this
        // allows for more stable readings with the ADS1115's internal PGA
        // enabled.
        //
        // Set comparator queue to disabled, since we won't be using the
        // comparator.
        m_ConfigReg.DR       = ads1115_config_DR_8SPS;
        m_ConfigReg.COMP_QUE = ads1115_config_COMP_QUE_DISABLE;
        WriteRegister(m_ConfigReg);

        ads1115_config_register_t config_reg_check = {.raw = 0};
        ReadRegister(config_reg_check);
        if (config_reg_check != m_ConfigReg) {
            ESP_LOGE(TAG,
                     "Config register verification failed: expected 0x%04x, "
                     "read back 0x%04x",
                     m_ConfigReg.raw,
                     config_reg_check.raw);
            throw std::runtime_error(
                std::format("ADS1115 config register verification failed: "
                            "expected 0x{:04x}, "
                            "read back 0x{:04x}",
                            m_ConfigReg.raw,
                            config_reg_check.raw));
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

    void WriteRegister(ads1115_conversion_register_t reg) {
        // The conversion register is read-only, so writing to it doesn't make
        // sense.
        throw std::invalid_argument(
            "Cannot write to conversion register - it is read-only");
    }

    struct I2CWireFormatConfig {
        uint8_t  register_address;
        uint16_t bits;
    };

    void WriteRegister(ads1115_config_register_t reg) {
        constexpr uint8_t  register_address = ADS1115_REG_CONFIG;
        ads1115_register_t in_reg           = {.address = {.P = register_address},
                                               .config  = reg};
        ads1115_log_register(ESP_LOG_INFO, &in_reg);
        I2CWireFormatConfig reg_to_write;
        reg_to_write.register_address = register_address;

        reg_to_write.bits = (reg.OS << 15);
        reg_to_write.bits |= (reg.MUX << 12);
        reg_to_write.bits |= (reg.PGA << 9);
        reg_to_write.bits |= (reg.MODE << 8);
        reg_to_write.bits |= (reg.DR << 5);
        reg_to_write.bits |= (reg.COMP_MODE << 4);
        reg_to_write.bits |= (reg.COMP_POL << 3);
        reg_to_write.bits |= (reg.COMP_LAT << 2);
        reg_to_write.bits |= reg.COMP_QUE;

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(&reg_to_write),
                           sizeof(reg_to_write),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_err != ESP_OK) {
            const char *const error_name = esp_err_to_name(write_err);
            ESP_LOGE(
                TAG, "Failed to write ADS1115 config register: %s", error_name);
            throw std::runtime_error(std::format(
                "Failed to write ADS1115 config register: {}", error_name));
        }
        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Wrote ADS1115 config register with value: 0x%04x",
                      reg_to_write.bits);

        m_ConfigReg = reg;
    }

    void WriteRegister(ads1115_lo_thresh_register_t reg) {
        constexpr uint8_t  register_address = ADS1115_REG_LO_THRESH;
        ads1115_register_t reg_to_write;
        reg_to_write.address.P       = register_address;
        reg_to_write.lo_thresh.value = ads1115_endian_swap_16(reg.value);

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(&reg_to_write),
                           sizeof(reg_to_write),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_err != ESP_OK) {
            const char *const error_name = esp_err_to_name(write_err);
            ESP_LOGE(TAG,
                     "Failed to write ADS1115 LO_THRESH register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to write ADS1115 LO_THRESH register: {}", error_name));
        }

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Wrote ADS1115 LO_THRESH register with value: 0x%04x",
                      reg.value);
    }

    void WriteRegister(ads1115_hi_thresh_register_t reg) {
        constexpr uint8_t  register_address = ADS1115_REG_HI_THRESH;
        ads1115_register_t reg_to_write;
        reg_to_write.address.P       = register_address;
        reg_to_write.hi_thresh.value = ads1115_endian_swap_16(reg.value);

        esp_err_t write_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(&reg_to_write),
                           sizeof(reg_to_write),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_err != ESP_OK) {
            const char *const error_name = esp_err_to_name(write_err);
            ESP_LOGE(TAG,
                     "Failed to write ADS1115 HI_THRESH register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to write ADS1115 HI_THRESH register: {}", error_name));
        }

        ESP_LOG_LEVEL(ADS1115::s_LogLevel,
                      TAG,
                      "Wrote ADS1115 HI_THRESH register with value: 0x%04x",
                      reg.value);
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

    ads1115_register_t
    ReadRegister(ads1115_conversion_register_t &conversion_reg) {
        // To read the conversion register, we need to write the register
        // address we want to read, then read the register value.
        constexpr uint8_t  register_address = ADS1115_REG_CONVERSION;
        ads1115_register_t reg_to_read;
        reg_to_read.address.P = register_address;

        esp_err_t write_request_err = m_pII2C->Write(
            m_Address,
            reinterpret_cast<const uint8_t *>(&reg_to_read.address),
            sizeof(ads1115_address_pointer_register_t),
            ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_request_err != ESP_OK) {
            const char *error_name = esp_err_to_name(write_request_err);
            ESP_LOGE(TAG,
                     "Failed to request ADS1115 conversion register: %s",
                     error_name);
            throw std::runtime_error(
                std::format("Failed to request ADS1115 conversion register: {}",
                            error_name));
        }

        esp_err_t read_err =
            m_pII2C->Read(m_Address,
                          reinterpret_cast<uint8_t *>(&reg_to_read.conversion),
                          sizeof(reg_to_read.conversion),
                          ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (read_err != ESP_OK) {
            const char *error_name = esp_err_to_name(read_err);
            ESP_LOGE(TAG,
                     "Failed to read ADS1115 conversion register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to read ADS1115 conversion register: {}", error_name));
        }

        conversion_reg.conversion_result = static_cast<int16_t>(
            ads1115_endian_swap_16(reg_to_read.conversion.conversion_result));
        reg_to_read.conversion = conversion_reg;

        ads1115_log_register(ADS1115::s_LogLevel, &reg_to_read);

        return reg_to_read;
    }

    ads1115_register_t ReadRegister(ads1115_config_register_t &config_reg) {
        I2CWireFormatConfig config_reg_wire;
        config_reg_wire.register_address = ADS1115_REG_CONFIG;
        config_reg_wire.bits             = 0;

        ESP_LOGD(TAG,
                 "Requesting ADS1115 config register by writing register "
                 "address 0x%02x",
                 config_reg_wire.register_address);

        // Read the config register by writing its address, then reading the
        // register value.
        esp_err_t write_request_config_err =
            m_pII2C->Write(m_Address,
                           reinterpret_cast<const uint8_t *>(
                               &config_reg_wire.register_address),
                           sizeof(ads1115_address_pointer_register_t),
                           ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_request_config_err != ESP_OK) {
            const char *error_name = esp_err_to_name(write_request_config_err);
            ESP_LOGE(TAG,
                     "Failed to request ADS1115 config register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to request ADS1115 config register: {}", error_name));
        }

        esp_err_t read_config_err =
            m_pII2C->Read(m_Address,
                          reinterpret_cast<uint8_t *>(&config_reg_wire.bits),
                          sizeof(config_reg_wire.bits),
                          ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (read_config_err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Failed to read ADS1115 config register: %s",
                     esp_err_to_name(read_config_err));
            throw std::runtime_error(
                std::format("Failed to read ADS1115 config register: {}",
                            esp_err_to_name(read_config_err)));
        }

        ESP_LOGD(TAG,
                 "Read raw config register value: 0x%04x",
                 config_reg_wire.bits);

        config_reg.OS        = (config_reg_wire.bits >> 15) & 0x1;
        config_reg.MUX       = (config_reg_wire.bits >> 12) & 0x7;
        config_reg.PGA       = (config_reg_wire.bits >> 9) & 0x7;
        config_reg.MODE      = (config_reg_wire.bits >> 8) & 0x1;
        config_reg.DR        = (config_reg_wire.bits >> 5) & 0x7;
        config_reg.COMP_MODE = (config_reg_wire.bits >> 4) & 0x1;
        config_reg.COMP_POL  = (config_reg_wire.bits >> 3) & 0x1;
        config_reg.COMP_LAT  = (config_reg_wire.bits >> 2) & 0x1;
        config_reg.COMP_QUE  = config_reg_wire.bits & 0x3;

        ads1115_register_t reg;
        reg.address.P = ADS1115_REG_CONFIG;
        reg.config    = config_reg;
        ads1115_log_register(ADS1115::s_LogLevel, &reg);
        return reg;
    }

    ads1115_register_t
    ReadRegister(ads1115_lo_thresh_register_t &lo_thresh_reg) {
        constexpr uint8_t  register_address = ADS1115_REG_LO_THRESH;
        ads1115_register_t reg_to_read;
        reg_to_read.address.P = register_address;

        esp_err_t write_request_err = m_pII2C->Write(
            m_Address,
            reinterpret_cast<const uint8_t *>(&reg_to_read.address),
            sizeof(ads1115_address_pointer_register_t),
            ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_request_err != ESP_OK) {
            const char *error_name = esp_err_to_name(write_request_err);
            ESP_LOGE(TAG,
                     "Failed to request ADS1115 LO_THRESH register: %s",
                     error_name);
            throw std::runtime_error(
                std::format("Failed to request ADS1115 LO_THRESH register: {}",
                            error_name));
        }

        esp_err_t read_err = m_pII2C->Read(
            m_Address,
            reinterpret_cast<uint8_t *>(&reg_to_read.lo_thresh.value),
            sizeof(reg_to_read.lo_thresh.value),
            ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (read_err != ESP_OK) {
            const char *error_name = esp_err_to_name(read_err);
            ESP_LOGE(TAG,
                     "Failed to read ADS1115 LO_THRESH register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to read ADS1115 LO_THRESH register: {}", error_name));
        }

        lo_thresh_reg.value =
            ads1115_endian_swap_16(reg_to_read.lo_thresh.value);
        reg_to_read.lo_thresh = lo_thresh_reg;

        ads1115_log_register(ADS1115::s_LogLevel, &reg_to_read);

        return reg_to_read;
    }

    ads1115_register_t
    ReadRegister(ads1115_hi_thresh_register_t &hi_thresh_reg) {
        constexpr uint8_t  register_address = ADS1115_REG_HI_THRESH;
        ads1115_register_t reg_to_read;
        reg_to_read.address.P = register_address;

        esp_err_t write_request_err = m_pII2C->Write(
            m_Address,
            reinterpret_cast<const uint8_t *>(&reg_to_read.address),
            sizeof(ads1115_address_pointer_register_t),
            ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (write_request_err != ESP_OK) {
            const char *error_name = esp_err_to_name(write_request_err);
            ESP_LOGE(TAG,
                     "Failed to request ADS1115 HI_THRESH register: %s",
                     error_name);
            throw std::runtime_error(
                std::format("Failed to request ADS1115 HI_THRESH register: {}",
                            error_name));
        }

        esp_err_t read_err = m_pII2C->Read(
            m_Address,
            reinterpret_cast<uint8_t *>(&reg_to_read.hi_thresh.value),
            sizeof(reg_to_read.hi_thresh.value),
            ADS1115::k_DEFAULT_TIMEOUT_MS);
        if (read_err != ESP_OK) {
            const char *error_name = esp_err_to_name(read_err);
            ESP_LOGE(TAG,
                     "Failed to read ADS1115 HI_THRESH register: %s",
                     error_name);
            throw std::runtime_error(std::format(
                "Failed to read ADS1115 HI_THRESH register: {}", error_name));
        }

        hi_thresh_reg.value =
            ads1115_endian_swap_16(reg_to_read.hi_thresh.value);
        reg_to_read.hi_thresh = hi_thresh_reg;

        ads1115_log_register(ADS1115::s_LogLevel, &reg_to_read);

        return reg_to_read;
    }

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