#include "kiwi2c/I2C.hpp"

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "hal/i2c_types.h"
#include "soc/gpio_num.h"

#include <cstdint>
#include <format>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

static const char *TAG = "kiwi::I2C";

namespace kiwi::i2c {

esp_log_level_t I2C::s_LogLevel = ESP_LOG_DEBUG;

struct I2C::Impl {

    Impl() = delete;

    static constexpr uint32_t I2C_MASTER_FREQ_HZ = 100000;

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
              .flags = {.enable_internal_pullup = true, .allow_pd = false},
          }
        , m_BusHandle{nullptr} {

        ESP_LOG_LEVEL_LOCAL(
            I2C::s_LogLevel,
            TAG,
            "Initializing I2C bus on port %d with SDA GPIO %d and SCL GPIO %d",
            m_BusConfig.i2c_port,
            m_BusConfig.sda_io_num,
            m_BusConfig.scl_io_num);

        ESP_ERROR_CHECK(i2c_new_master_bus(&m_BusConfig, &m_BusHandle));

        ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                            TAG,
                            "I2C bus (%p) initialized successfully on port %d "
                            "with SDA GPIO %d and SCL GPIO %d",
                            m_BusHandle,
                            m_BusConfig.i2c_port,
                            m_BusConfig.sda_io_num,
                            m_BusConfig.scl_io_num);
    }

    ~Impl() {
        for (const auto &[address, devHandle] : m_DeviceHandleMap) {

            ESP_LOG_LEVEL_LOCAL(
                I2C::s_LogLevel,
                TAG,
                "Removing I2C device with address 0x%02X from bus %p",
                address,
                m_BusHandle);

            ESP_ERROR_CHECK(i2c_master_bus_rm_device(devHandle));

            ESP_LOG_LEVEL_LOCAL(I2C::s_LogLevel,
                                TAG,
                                "I2C device with address 0x%02X removed "
                                "successfully from bus %p",
                                address,
                                m_BusHandle);
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
            throw std::invalid_argument(std::format(
                "I2C device with address 0x{:02X} not found on bus {:p}",
                address,
                (void *)m_BusHandle));
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
                        "Adding I2C device with address 0x%02X to bus %p",
                        address,
                        pImpl->m_BusHandle);

    i2c_device_config_t devConfig = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                     .device_address  = address,
                                     .scl_speed_hz = Impl::I2C_MASTER_FREQ_HZ,
                                     .scl_wait_us  = 0,
                                     .flags = {.disable_ack_check = false}};

    i2c_master_dev_handle_t devHandle = {0};

    esp_err_t result =
        i2c_master_bus_add_device(pImpl->m_BusHandle, &devConfig, &devHandle);

    if (result != ESP_OK) {
        throw std::runtime_error(std::format(
            "Failed to add I2C device with address 0x{:02X} to bus {:p}",
            address,
            GetBusHandle()));
    }

    if (!devHandle) {
        throw std::runtime_error(std::format(
            "Failed to add I2C device with address 0x{:02X} to bus {:p}: "
            "devHandle is nullptr",
            address,
            GetBusHandle()));
    }

    ESP_LOGD(TAG,
             "I2C device with address 0x%02X added to bus %p: devHandle=%p",
             address,
             pImpl->m_BusHandle,
             devHandle);

    pImpl->m_DeviceHandleMap[address] = devHandle;

    ESP_LOG_LEVEL_LOCAL(
        I2C::s_LogLevel,
        TAG,
        "I2C device with address 0x%02X added successfully to bus %p",
        address,
        pImpl->m_BusHandle);

    return result;
}

esp_err_t I2C::Write(uint8_t        address,
                     const uint8_t *data,
                     size_t         size,
                     uint32_t       timeout_ms) {

    i2c_master_dev_handle_t devHandle = pImpl->GetDeviceHandle(address);

    if (devHandle == nullptr) {
        throw std::invalid_argument(
            std::format("Write to I2C device with address 0x{:02X} failed: "
                        "GetDeviceHandle returned nullptr",
                        address));
    }

    ESP_LOGD(TAG, "Writing to I2C device with address 0x%02X", address);

#if !defined(NDEBUG)
    {
        std::string s(size * 3 - 1, '_');
        for (size_t i = 0; i < size; ++i) {
            std::snprintf(&s[i * 3], 4, "%02X ", data[i]);
        }
        ESP_LOGD(TAG,
                 "Writing (bytes) %p - %p: [%s]",
                 &data[0],
                 &data[size - 1],
                 s.c_str());
    }
#endif

    esp_err_t result = i2c_master_transmit(devHandle, data, size, timeout_ms);
    return result;
}

esp_err_t
I2C::Read(uint8_t address, uint8_t *data, size_t size, uint32_t timeout_ms) {
    i2c_master_dev_handle_t devHandle = pImpl->GetDeviceHandle(address);
    if (devHandle == nullptr) {
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR,
                            TAG,
                            "%s%u: Read from I2C device with address 0x%02X "
                            "failed: GetDeviceHandle returned nullptr",
                            __FILE__,
                            __LINE__,
                            address);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Reading from I2C device with address 0x%02X", address);

    esp_err_t result = i2c_master_receive(devHandle, data, size, timeout_ms);

#if !defined(NDEBUG)
    {
        std::string s(size * 3 - 1, '_');
        for (size_t i = 0; i < size; ++i) {
            std::snprintf(&s[i * 3], 4, "%02X ", data[i]);
        }
        ESP_LOGD(TAG,
                 "Read (bytes) %p - %p: [%s]",
                 &data[0],
                 &data[size - 1],
                 s.c_str());
    }
#endif

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
        throw std::invalid_argument(
            std::format("WriteRead to I2C device on bus {:p} with address "
                        "0x{:02X} failed: GetDeviceHandle returned nullptr",
                        GetBusHandle(),
                        address));
    }

    ESP_LOGD(TAG,
             "WriteRead to I2C device %p with address 0x%02X on bus %p",
             devHandle,
             address,
             GetBusHandle());

#if !defined(NDEBUG)
    {
        std::string s(write_size * 3 - 1, '_');
        for (size_t i = 0; i < write_size; ++i) {
            std::snprintf(&s[i * 3], 4, "%02X ", write_data[i]);
        }

        ESP_LOGD(TAG,
                 "WriteRead writing (bytes) %p - %p: [%s]",
                 &write_data[0],
                 &write_data[write_size - 1],
                 s.c_str());
    }
#endif

    esp_err_t result = i2c_master_transmit_receive(
        devHandle, write_data, write_size, read_data, read_size, timeout_ms);

#if !defined(NDEBUG)
    {
        std::string s(read_size * 3 - 1, '_');
        for (size_t i = 0; i < read_size; ++i) {
            std::snprintf(&s[i * 3], 4, "%02X ", read_data[i]);
        }
        ESP_LOGD(TAG,
                 "WriteRead read (bytes) %p - %p: [%s]",
                 &read_data[0],
                 &read_data[read_size - 1],
                 s.c_str());
    }
#endif
    return result;
}

} // namespace kiwi::i2c
