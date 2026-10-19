#include "kiwi2c/I2CFactory.hpp"

#if I2CDEV_USE_LEGACY_DRIVER
#include "kiwi2c/I2CLegacy.hpp"
#else
#include "kiwi2c/I2C.hpp"
#endif

#include "esp_log.h"

#include <memory>

namespace kiwi::i2c::factory {

static constexpr const char *TAG = "kiwi::i2c::factory";

std::shared_ptr<II2C>
CreateI2C(I2CDriver driver, uint8_t bus, uint8_t sda_gpio, uint8_t scl_gpio) {
    switch (driver) {
#ifdef I2CDEV_USE_LEGACY_DRIVER
    case I2CDriver::Legacy:
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO,
                            TAG,
                            "Creating legacy I2C instance "
                            "on bus %u (SDA GPIO: %u, SCL GPIO: %u)",
                            bus,
                            sda_gpio,
                            scl_gpio);
        return std::make_shared<i2c::legacy::I2C>(bus, sda_gpio, scl_gpio);
#else
    case I2CDriver::New:
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO,
                            TAG,
                            "Creating new I2C instance "
                            "on bus %u (SDA GPIO: %u, SCL GPIO: %u)",
                            bus,
                            sda_gpio,
                            scl_gpio);
        return std::make_shared<i2c::I2C>(bus, sda_gpio, scl_gpio);
#endif
    default:
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, TAG, "Unknown I2C driver specified");
        return nullptr;
    }
}

} // namespace kiwi::i2c::factory
