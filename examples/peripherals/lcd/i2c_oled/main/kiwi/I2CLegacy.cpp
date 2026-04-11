#include "I2CLegacy.hpp"

#include <driver/i2c.h>
#include <esp_err.h>
#include <esp_log.h>

#include <memory>
#include <optional>

namespace kiwi::i2c::legacy {

namespace {
static const char *const TAG = "kiwi::i2c::legacy";
} // namespace

esp_log_level_t I2C::s_LogLevel = ESP_LOG_INFO;

struct I2C::Impl {
    Impl() = delete;

    static constexpr i2c_clock_source_t k_I2C_CLOCK_SOURCE =
        I2C_CLK_SRC_DEFAULT;
    static constexpr uint32_t k_I2C_MASTER_FREQ_HZ = 100000; // 100kHz

    static constexpr uint32_t k_slv_rx_buf_len   = 0;
    static constexpr uint32_t k_slv_tx_buf_len   = 0;
    static constexpr int      k_intr_alloc_flags = 0;

    Impl(uint8_t    bus,
         i2c_port_t port_num,
         gpio_num_t sda_gpio,
         gpio_num_t scl_gpio)
        : m_PortNum(port_num)
        , m_Config{.mode          = I2C_MODE_MASTER,
                   .sda_io_num    = sda_gpio,
                   .scl_io_num    = scl_gpio,
                   .sda_pullup_en = GPIO_PULLUP_ENABLE,
                   .scl_pullup_en = GPIO_PULLUP_ENABLE,
                   .master        = {.clk_speed = Impl::k_I2C_MASTER_FREQ_HZ},
                   .clk_flags     = 0} {

        ESP_LOG_LEVEL_LOCAL(
            I2C::s_LogLevel,
            TAG,
            "Initializing legacy I2C bus on port %d with SDA GPIO %d and "
            "SCL GPIO %d",
            port_num,
            sda_gpio,
            scl_gpio);

        ESP_ERROR_CHECK(i2c_param_config(port_num, &m_Config));
        ESP_ERROR_CHECK(i2c_driver_install(port_num,
                                           m_Config.mode,
                                           k_slv_rx_buf_len,
                                           k_slv_tx_buf_len,
                                           k_intr_alloc_flags));

        ESP_LOG_LEVEL_LOCAL(
            I2C::s_LogLevel,
            TAG,
            "Legacy I2C bus initialized successfully on port %d",
            port_num);
    }

    ~Impl() {
        ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                            TAG,
                            "Deleting legacy I2C driver for port %d",
                            m_PortNum);

        ESP_ERROR_CHECK(i2c_driver_delete(m_PortNum));

        ESP_LOGI(TAG,
                 "Legacy I2C driver deleted successfully for port %d",
                 m_PortNum);
    }

    i2c_port_t   m_PortNum;
    i2c_config_t m_Config;
};

I2C::I2C(uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio)
    : pImpl(std::make_unique<Impl>(bus,
                                   static_cast<i2c_port_t>(bus),
                                   static_cast<gpio_num_t>(sda_gpio),
                                   static_cast<gpio_num_t>(scl_gpio))) {
}

I2C::~I2C() {
}

uint32_t I2C::GetBusNumber() const {
    return static_cast<uint32_t>(pImpl->m_PortNum);
}

void *I2C::GetBusHandle() const {
    return &(pImpl->m_PortNum);
}

uint32_t I2C::GetSDA_GPIO() const {
    return static_cast<uint32_t>(pImpl->m_Config.sda_io_num);
}

uint32_t I2C::GetSCL_GPIO() const {
    return static_cast<uint32_t>(pImpl->m_Config.scl_io_num);
}

esp_err_t I2C::AddBusDevice(uint8_t address) {

    return ESP_OK;
}

esp_err_t I2C::Write(uint8_t        address,
                     const uint8_t *data,
                     size_t         size,
                     uint32_t       timeout_ms) {

    ESP_LOG_LEVEL_LOCAL(
        I2C::s_LogLevel,
        TAG,
        "Writing %d bytes to device at address 0x%02X on legacy I2C bus",
        size,
        address);

    esp_err_t result = i2c_master_write_to_device(
        pImpl->m_PortNum, address, data, size, pdMS_TO_TICKS(timeout_ms));
    return result;
}

esp_err_t
I2C::Read(uint8_t address, uint8_t *data, size_t size, uint32_t timeout_ms) {

    ESP_LOG_LEVEL_LOCAL(
        I2C::s_LogLevel,
        TAG,
        "Reading %d bytes from device at address 0x%02X on legacy I2C bus",
        size,
        address);

    esp_err_t result = i2c_master_read_from_device(
        pImpl->m_PortNum, address, data, size, pdMS_TO_TICKS(timeout_ms));
    return result;
}

esp_err_t I2C::WriteRead(uint8_t        address,
                         const uint8_t *write_data,
                         size_t         write_size,
                         uint8_t       *read_data,
                         size_t         read_size,
                         uint32_t       timeout_ms) {

    ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                        TAG,
                        "Performing write-read operation with %d bytes to "
                        "write and %d bytes to read "
                        "for device at address 0x%02X on legacy I2C bus",
                        write_size,
                        read_size,
                        address);

    esp_err_t result = i2c_master_write_read_device(pImpl->m_PortNum,
                                                    address,
                                                    write_data,
                                                    write_size,
                                                    read_data,
                                                    read_size,
                                                    pdMS_TO_TICKS(timeout_ms));
    return result;
}

} // namespace kiwi::i2c::legacy