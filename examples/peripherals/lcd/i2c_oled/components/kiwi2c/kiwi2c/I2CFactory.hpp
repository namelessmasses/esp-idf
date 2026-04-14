#pragma once

#include "kiwi2c/II2C.hpp"

#include <memory>

namespace kiwi::i2c::factory {

enum class I2CDriver { Legacy, New };

std::shared_ptr<II2C>
CreateI2C(I2CDriver driver, uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio);

} // namespace kiwi::i2c::factory
