#include "../../II2C.hpp"

#include <esp_log_level.h>

#include <cstdint>
#include <memory>
#include <sys/stat.h>

namespace kiwi::i2c {

class ADS1115 {
  public:
    static constexpr uint8_t  k_DEFAULT_I2C_ADDRESS = 0x48;
    static constexpr uint32_t k_DEFAULT_TIMEOUT_MS  = 100;

    /**
     * Defaults to ESP_LOG_DEBUG
     */
    static esp_log_level_t s_LogLevel;

    ADS1115()                           = delete;
    ADS1115(const ADS1115 &)            = delete;
    ADS1115(ADS1115 &&)                 = delete;
    ADS1115 &operator=(const ADS1115 &) = delete;
    ADS1115 &operator=(ADS1115 &&)      = delete;

    ~ADS1115();

    enum class PGA {
        FS_6_144V = 0,
        FS_4_096V = 1,
        FS_2_048V = 2,
        FS_1_024V = 3,
        FS_0_512V = 4,
        FS_0_256V = 5,
        DEFAULT = FS_2_048V
    };

    static constexpr PGA DEFAULT_PGA = PGA::DEFAULT;

    enum class MUX {
        AIN0_AIN1 = 0,
        AIN0_AIN3 = 1,
        AIN1_AIN3 = 2,
        AIN2_AIN3 = 3,
        AIN0_GND  = 4,
        AIN1_GND  = 5,
        AIN2_GND  = 6,
        AIN3_GND  = 7,
        DEFAULT = AIN0_AIN1
    };

    static constexpr MUX DEFAULT_MUX = MUX::DEFAULT;

    ADS1115(std::shared_ptr<II2C> i2c,
            uint8_t               address = k_DEFAULT_I2C_ADDRESS,
            PGA                   pga     = DEFAULT_PGA,
            MUX                   mux     = DEFAULT_MUX);

    void SetPGA(PGA pga);

    void SetMUX(MUX mux);

    float GetVoltage();

  private:
    class Impl;

    std::unique_ptr<Impl> m_pImpl;
};

} // namespace kiwi::i2c
