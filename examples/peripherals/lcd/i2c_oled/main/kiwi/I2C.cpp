#include "I2C.hpp"

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "hal/i2c_types.h"
#include "soc/gpio_num.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

static const char *TAG = "kiwi::I2C";

namespace kiwi::i2c {

esp_log_level_t I2C::s_LogLevel = ESP_LOG_INFO;

struct I2C::Impl {

    Impl() = delete;

    static constexpr uint32_t I2C_MASTER_FREQ_HZ = 400000;

    Impl(uint8_t        bus,
         i2c_port_num_t port_num,
         gpio_num_t     sda_gpio,
         gpio_num_t     scl_gpio)
        : m_BusConfig{
              .i2c_port          = port_num,
              .sda_io_num        = sda_gpio,
              .scl_io_num        = scl_gpio,
              .clk_source        = I2C_CLK_SRC_DEFAULT,
              .glitch_ignore_cnt = 7,
              .intr_priority     = 0,
              .trans_queue_depth = 0,
              .flags = {.enable_internal_pullup = true, .allow_pd = false}} {

        ESP_LOG_LEVEL_LOCAL(
            I2C::s_LogLevel,
            TAG,
            "Initializing I2C bus on port %d with SDA GPIO %d and SCL GPIO %d",
            m_BusConfig.i2c_port,
            m_BusConfig.sda_io_num,
            m_BusConfig.scl_io_num);

        ESP_ERROR_CHECK(i2c_new_master_bus(&m_BusConfig, &m_BusHandle));
    }

    ~Impl() {
        for (const auto &[address, devHandle] : m_DeviceHandleMap) {
            ESP_LOG_LEVEL_LOCAL(
                I2C::s_LogLevel,
                TAG,
                "Removing I2C device with address 0x%02X from bus",
                address);
            ESP_ERROR_CHECK(i2c_master_bus_rm_device(devHandle));
            ESP_LOG_LEVEL_LOCAL(
                I2C::s_LogLevel,
                TAG,
                "I2C device with address 0x%02X removed successfully from bus",
                address);
        }
        ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                            TAG,
                            "Deleting I2C bus on port %d",
                            m_BusConfig.i2c_port);
        ESP_ERROR_CHECK(i2c_del_master_bus(m_BusHandle));
        ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                            TAG,
                            "I2C bus on port %d deleted successfully",
                            m_BusConfig.i2c_port);
    }

    i2c_master_bus_config_t m_BusConfig;
    i2c_master_bus_handle_t m_BusHandle;

    using device_handle_map_type =
        std::unordered_map<uint8_t, i2c_master_dev_handle_t>;
    device_handle_map_type m_DeviceHandleMap;

    i2c_master_dev_handle_t GetDeviceHandle(uint8_t address) const {
        auto it = m_DeviceHandleMap.find(address);
        if (it == m_DeviceHandleMap.end()) {
            return nullptr;
        }
        return it->second;
    }
};

I2C::I2C(uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio)
    : pImpl(std::make_unique<Impl>(bus,
                                   static_cast<i2c_port_num_t>(bus),
                                   static_cast<gpio_num_t>(sda_gpio),
                                   static_cast<gpio_num_t>(scl_gpio))) {
}

I2C::~I2C() {
}

uint32_t I2C::GetBusNumber() const {
    return pImpl->m_BusConfig.i2c_port;
}

void *I2C::GetBusHandle() const {
    return pImpl->m_BusHandle;
}

uint32_t I2C::GetSDA_GPIO() const {
    return pImpl->m_BusConfig.sda_io_num;
}

uint32_t I2C::GetSCL_GPIO() const {
    return pImpl->m_BusConfig.scl_io_num;
}

esp_err_t I2C::AddBusDevice(uint8_t address) {
    ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                        TAG,
                        "Adding I2C device with address 0x%02X to bus",
                        address);
    i2c_device_config_t devConfig = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                     .device_address  = address,
                                     .scl_speed_hz = Impl::I2C_MASTER_FREQ_HZ,
                                     .scl_wait_us  = 0,
                                     .flags = {.disable_ack_check = false}};

    i2c_master_dev_handle_t devHandle;
    esp_err_t result = i2c_master_bus_add_device(
        pImpl->m_BusHandle, &devConfig, &devHandle);

    if (result == ESP_OK) {

        pImpl->m_DeviceHandleMap[address] = devHandle;
        ESP_LOG_LEVEL_LOCAL(
            I2C::s_LogLevel,
            TAG,
            "I2C device with address 0x%02X added successfully to bus",
            address);
    }

    return result;
}

esp_err_t I2C::Write(uint8_t        address,
                     const uint8_t *data,
                     size_t         size,
                     uint32_t       timeout_ms) {
    i2c_master_dev_handle_t devHandle = pImpl->GetDeviceHandle(address);
    if (devHandle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = i2c_master_transmit(devHandle, data, size, timeout_ms);
    return result;
}

esp_err_t
I2C::Read(uint8_t address, uint8_t *data, size_t size, uint32_t timeout_ms) {
    i2c_master_dev_handle_t devHandle = pImpl->GetDeviceHandle(address);
    if (devHandle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = i2c_master_receive(devHandle, data, size, timeout_ms);
    return result;
}

esp_err_t I2C::WriteRead(uint8_t        address,
                         const uint8_t *write_data,
                         size_t         write_size,
                         uint8_t       *read_data,
                         size_t         read_size,
                         uint32_t       timeout_ms) {
    i2c_master_dev_handle_t devHandle = pImpl->GetDeviceHandle(address);
    if (devHandle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = i2c_master_transmit_receive(
        devHandle, write_data, write_size, read_data, read_size, timeout_ms);
    return result;
}

} // namespace kiwi::i2c
