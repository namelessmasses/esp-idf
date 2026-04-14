#pragma once

#include "kiwi2c/II2C.hpp"

#include "sht41_reading.hpp"

#include <cstdint>
#include <memory>

namespace kiwi::i2c {

class SHT41 {
  public:
    constexpr static uint8_t DEFAULT_I2C_ADDRESS = 0x44;

    SHT41()                         = delete;
    SHT41(const SHT41 &)            = delete;
    SHT41(SHT41 &&)                 = delete;
    SHT41 &operator=(const SHT41 &) = delete;
    SHT41 &operator=(SHT41 &&)      = delete;

    ~SHT41();

    SHT41(std::shared_ptr<II2C> const &i2c,
          uint8_t                      address = DEFAULT_I2C_ADDRESS);

    using Reading = sht41_sensor_data_t;

    Reading GetReading();

  private:
    class Impl;

    std::unique_ptr<Impl> pImpl;
};

} // namespace kiwi::i2c