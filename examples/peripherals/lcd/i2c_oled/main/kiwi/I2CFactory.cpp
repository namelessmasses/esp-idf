#include "I2CFactory.hpp"

#include "I2C.hpp"
#include "I2CLegacy.hpp"

#include <memory>

namespace kiwi::i2c::factory {

std::shared_ptr<II2C>
CreateI2C(I2CDriver driver, uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio) {
    switch (driver) {
    case I2CDriver::Legacy:
#ifdef I2CDEV_USE_LEGACY_DRIVER
        return std::make_shared<i2c::legacy::I2C>(bus, sda_gpio, scl_gpio);
#else
        return nullptr;
#endif
    case I2CDriver::New:
        return std::make_shared<i2c::I2C>(bus, sda_gpio, scl_gpio);
    default:
        return nullptr;
    }
}

} // namespace kiwi::i2c::factory
