#pragma once

#include "../../II2C.hpp"

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

    struct Reading {
        float temperature_celcius;
        float temperature_fahrenheit;
        float relative_humidity;
    };

    Reading GetReading();

  private:
    class Impl;

    std::unique_ptr<Impl> pImpl;
};

} // namespace kiwi::i2c