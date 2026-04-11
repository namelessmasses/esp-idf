#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace kiwi::i2c {

class II2C {
  protected:
    II2C()                        = default;
    II2C(const II2C &)            = default;
    II2C &operator=(const II2C &) = default;

  public:
    virtual ~II2C() = default;

    virtual uint32_t GetBusNumber() const = 0;

    virtual void *GetBusHandle() const = 0;

    virtual uint32_t GetSDA_GPIO() const = 0;

    virtual uint32_t GetSCL_GPIO() const = 0;

    virtual esp_err_t AddBusDevice(uint8_t address) = 0;

    virtual esp_err_t Write(uint8_t        address,
                            const uint8_t *data,
                            size_t         size,
                            uint32_t       timeout_ms) = 0;

    virtual esp_err_t
    Read(uint8_t address, uint8_t *data, size_t size, uint32_t timeout_ms) = 0;

    virtual esp_err_t WriteRead(uint8_t        address,
                                const uint8_t *write_data,
                                size_t         write_size,
                                uint8_t       *read_data,
                                size_t         read_size,
                                uint32_t       timeout_ms) = 0;
};

} // namespace kiwi