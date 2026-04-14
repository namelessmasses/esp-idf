#include "kiwi2c/I2CFactory.hpp"

#if I2CDEV_USE_LEGACY_DRIVER
#include "kiwi2c/I2CLegacy.hpp"
#else
#include "kiwi2c/I2C.hpp"
#endif

#include <memory>

namespace kiwi::i2c::factory {

std::shared_ptr<II2C>
CreateI2C(I2CDriver driver, uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio) {
    switch (driver) {
#ifdef I2CDEV_USE_LEGACY_DRIVER
    case I2CDriver::Legacy:
        return std::make_shared<i2c::legacy::I2C>(bus, sda_gpio, scl_gpio);
#else
    case I2CDriver::New:
        return std::make_shared<i2c::I2C>(bus, sda_gpio, scl_gpio);
#endif
    default:
        return nullptr;
    }
}

} // namespace kiwi::i2c::factory
