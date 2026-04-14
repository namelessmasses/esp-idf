#pragma once

#include "kiwi2c/II2C.hpp"

#include <esp_log_level.h>

#include <memory>

namespace kiwi::i2c::legacy {

class I2C : public II2C {
  public:
    static esp_log_level_t s_LogLevel;

    I2C() = delete;

    virtual ~I2C() override;

    I2C(uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio);

    uint32_t GetBusNumber() const override;
    void *GetBusHandle() const override;
    uint32_t GetSDA_GPIO() const override;
    uint32_t GetSCL_GPIO() const override;

    esp_err_t AddBusDevice(uint8_t address) override;

    esp_err_t Write(uint8_t        address,
                    const uint8_t *data,
                    size_t         size,
                    uint32_t       timeout_ms) override;

    esp_err_t Read(uint8_t  address,
                   uint8_t *data,
                   size_t   size,
                   uint32_t timeout_ms) override;

    esp_err_t WriteRead(uint8_t        address,
                        const uint8_t *write_data,
                        size_t         write_size,
                        uint8_t       *read_data,
                        size_t         read_size,
                        uint32_t       timeout_ms) override;

  private:
    struct Impl;

    std::unique_ptr<Impl> pImpl;
};

} // namespace kiwi::i2c::legacy
