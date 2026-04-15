extern "C" {
#include <unity.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>
}

#include "ADS1115.hpp"
#include "kiwi2c/II2C.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

namespace {

struct test_successful_halt : std::exception {
    const char *what() const noexcept override {
        return "Test successful - halting execution";
    }
};
} // namespace

struct MockI2C : public kiwi::i2c::II2C {

    using write_callback_type = std::function<esp_err_t(uint8_t        address,
                                                        const uint8_t *data,
                                                        size_t     data_length,
                                                        TickType_t timeout_ms)>;

    using read_callback_type = std::function<esp_err_t(uint8_t    address,
                                                       uint8_t   *data,
                                                       size_t     data_length,
                                                       TickType_t timeout_ms)>;

    using write_read_callback_type =
        std::function<esp_err_t(uint8_t        address,
                                const uint8_t *write_data,
                                size_t         write_data_length,
                                uint8_t       *read_data,
                                size_t         read_data_length,
                                TickType_t     timeout_ms)>;

    using add_bus_device_callback_type =
        std::function<esp_err_t(uint8_t address)>;

    using get_bus_number_callback_type = std::function<uint32_t()>;

    using get_bus_handle_callback_type = std::function<void *()>;

    using get_sda_gpio_callback_type = std::function<uint32_t()>;

    using get_scl_gpio_callback_type = std::function<uint32_t()>;

    static write_callback_type s_fn_default_write_callback;

    static read_callback_type s_fn_default_read_callback;

    static write_read_callback_type s_fn_default_write_read_callback;

    static add_bus_device_callback_type s_fn_default_add_bus_device_callback;

    static get_bus_number_callback_type s_fn_default_get_bus_number_callback;

    static get_bus_handle_callback_type s_fn_default_get_bus_handle_callback;

    static get_sda_gpio_callback_type s_fn_default_get_sda_gpio_callback;

    static get_scl_gpio_callback_type s_fn_default_get_scl_gpio_callback;

    std::deque<write_callback_type>          m_fn_write_callback;
    std::deque<read_callback_type>           m_fn_read_callback;
    std::deque<write_read_callback_type>     m_fn_write_read_callback;
    std::deque<add_bus_device_callback_type> m_fn_add_bus_device_callback;
    std::deque<get_bus_number_callback_type> m_fn_get_bus_number_callback;
    std::deque<get_bus_handle_callback_type> m_fn_get_bus_handle_callback;
    std::deque<get_sda_gpio_callback_type>   m_fn_get_sda_gpio_callback;
    std::deque<get_scl_gpio_callback_type>   m_fn_get_scl_gpio_callback;

    MockI2C()
        : m_fn_write_callback()
        , m_fn_read_callback()
        , m_fn_write_read_callback()
        , m_fn_add_bus_device_callback()
        , m_fn_get_bus_number_callback()
        , m_fn_get_bus_handle_callback()
        , m_fn_get_sda_gpio_callback()
        , m_fn_get_scl_gpio_callback() {
    }

    void validate() {
        if (!m_fn_write_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::Write were "
                              "made during the test");
        }

        if (!m_fn_read_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::Read were "
                              "made during the test");
        }

        if (!m_fn_write_read_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::WriteRead "
                              "were made during the test");
        }

        if (!m_fn_add_bus_device_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::AddBusDevice "
                              "were made during the test");
        }

        if (!m_fn_get_bus_number_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::GetBusNumber "
                              "were made during the test");
        }

        if (!m_fn_get_bus_handle_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::GetBusHandle "
                              "were made during the test");
        }

        if (!m_fn_get_sda_gpio_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::GetSDAGPIO "
                              "were made during the test");
        }

        if (!m_fn_get_scl_gpio_callback.empty()) {
            TEST_FAIL_MESSAGE("Not all expected calls to MockI2C::GetSCLGPIO "
                              "were made during the test");
        }
    }

    ~MockI2C() {
        validate();
    }

    void ExpectWrite(write_callback_type fn) {
        m_fn_write_callback.push_back(fn);
    }

    void ExpectRead(read_callback_type fn) {
        m_fn_read_callback.push_back(fn);
    }

    void ExpectWriteRead(write_read_callback_type fn) {
        m_fn_write_read_callback.push_back(fn);
    }

    void ExpectAddBusDevice(add_bus_device_callback_type fn) {
        m_fn_add_bus_device_callback.push_back(fn);
    }

    void ExpectGetBusNumber(get_bus_number_callback_type fn) {
        m_fn_get_bus_number_callback.push_back(fn);
    }

    void ExpectGetBusHandle(get_bus_handle_callback_type fn) {
        m_fn_get_bus_handle_callback.push_back(fn);
    }

    void ExpectGetSDAGPIO(get_sda_gpio_callback_type fn) {
        m_fn_get_sda_gpio_callback.push_back(fn);
    }

    void ExpectGetSCLGPIO(get_scl_gpio_callback_type fn) {
        m_fn_get_scl_gpio_callback.push_back(fn);
    }

    esp_err_t Write(uint8_t        address,
                    const uint8_t *data,
                    size_t         data_length,
                    TickType_t     timeout_ms) override {

        if (m_fn_write_callback.empty()) {
            return s_fn_default_write_callback(
                address, data, data_length, timeout_ms);
        }

        auto fn = m_fn_write_callback.front();
        m_fn_write_callback.pop_front();
        return fn(address, data, data_length, timeout_ms);
    }

    esp_err_t Read(uint8_t    address,
                   uint8_t   *data,
                   size_t     data_length,
                   TickType_t timeout_ms) override {
        if (m_fn_read_callback.empty()) {
            return s_fn_default_read_callback(
                address, data, data_length, timeout_ms);
        }
        auto fn = m_fn_read_callback.front();
        m_fn_read_callback.pop_front();
        return fn(address, data, data_length, timeout_ms);
    }

    esp_err_t WriteRead(uint8_t        address,
                        const uint8_t *write_data,
                        size_t         write_data_length,
                        uint8_t       *read_data,
                        size_t         read_data_length,
                        TickType_t     timeout_ms) override {
        if (m_fn_write_read_callback.empty()) {
            return s_fn_default_write_read_callback(address,
                                                    write_data,
                                                    write_data_length,
                                                    read_data,
                                                    read_data_length,
                                                    timeout_ms);
        }
        write_read_callback_type fn = m_fn_write_read_callback.front();
        m_fn_write_read_callback.pop_front();
        return fn(address,
                  write_data,
                  write_data_length,
                  read_data,
                  read_data_length,
                  timeout_ms);
    }

    esp_err_t AddBusDevice(uint8_t address) override {
        if (m_fn_add_bus_device_callback.empty()) {
            return s_fn_default_add_bus_device_callback(address);
        }
        add_bus_device_callback_type fn = m_fn_add_bus_device_callback.front();
        m_fn_add_bus_device_callback.pop_front();
        return fn(address);
    }

    uint32_t GetBusNumber() const override {
        if (m_fn_get_bus_number_callback.empty()) {
            return s_fn_default_get_bus_number_callback();
        }
        get_bus_number_callback_type fn = m_fn_get_bus_number_callback.front();
        const_cast<std::deque<get_bus_number_callback_type> &>(
            m_fn_get_bus_number_callback)
            .pop_front();
        return fn();
    }

    void *GetBusHandle() const override {
        if (m_fn_get_bus_handle_callback.empty()) {
            return s_fn_default_get_bus_handle_callback();
        }
        get_bus_handle_callback_type fn = m_fn_get_bus_handle_callback.front();
        const_cast<std::deque<get_bus_handle_callback_type> &>(
            m_fn_get_bus_handle_callback)
            .pop_front();
        return fn();
    }

    uint32_t GetSDA_GPIO() const override {
        if (m_fn_get_sda_gpio_callback.empty()) {
            return s_fn_default_get_sda_gpio_callback();
        }
        get_sda_gpio_callback_type fn = m_fn_get_sda_gpio_callback.front();
        const_cast<std::deque<get_sda_gpio_callback_type> &>(
            m_fn_get_sda_gpio_callback)
            .pop_front();
        return fn();
    }

    uint32_t GetSCL_GPIO() const override {
        if (m_fn_get_scl_gpio_callback.empty()) {
            return s_fn_default_get_scl_gpio_callback();
        }
        get_scl_gpio_callback_type fn = m_fn_get_scl_gpio_callback.front();
        const_cast<std::deque<get_scl_gpio_callback_type> &>(
            m_fn_get_scl_gpio_callback)
            .pop_front();
        return fn();
    }
};

MockI2C::write_callback_type MockI2C::s_fn_default_write_callback =
    [](uint8_t, const uint8_t *, size_t, TickType_t) {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::Write with default callback");
        return ESP_OK;
    };

MockI2C::read_callback_type MockI2C::s_fn_default_read_callback =
    [](uint8_t, uint8_t *, size_t, TickType_t) {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::Read with default callback");
        return ESP_OK;
    };

MockI2C::write_read_callback_type MockI2C::s_fn_default_write_read_callback =
    [](uint8_t, const uint8_t *, size_t, uint8_t *, size_t, TickType_t) {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::WriteRead with default callback");
        return ESP_OK;
    };

MockI2C::add_bus_device_callback_type
    MockI2C::s_fn_default_add_bus_device_callback = [](uint8_t) {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::AddBusDevice with default callback");
        return ESP_OK;
    };

MockI2C::get_bus_number_callback_type
    MockI2C::s_fn_default_get_bus_number_callback = []() {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::GetBusNumber with default callback");
        return 0;
    };

MockI2C::get_bus_handle_callback_type
    MockI2C::s_fn_default_get_bus_handle_callback = []() {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::GetBusHandle with default callback");
        return nullptr;
    };

MockI2C::get_sda_gpio_callback_type
    MockI2C::s_fn_default_get_sda_gpio_callback = []() {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::GetSDAGPIO with default callback");
        return 0;
    };

MockI2C::get_scl_gpio_callback_type
    MockI2C::s_fn_default_get_scl_gpio_callback = []() {
        TEST_FAIL_MESSAGE(
            "Unexpected call to MockI2C::GetSCLGPIO with default callback");
        return 0;
    };

TEST_CASE("[ADS1115].Initialization.null_i2c_throws",
          "Null I2C interface should throw") {
    std::shared_ptr<kiwi::i2c::II2C> p_null_i2c = nullptr;

    using kiwi::i2c::ADS1115;

    try {
        ADS1115 ads1115(p_null_i2c);
        TEST_FAIL_MESSAGE(
            "ADS1115 constructor should throw when given a null I2C interface");
    } catch (const std::invalid_argument &e) {
        TEST_ASSERT_EQUAL_STRING("I2C interface cannot be null", e.what());
    } catch (...) {
        TEST_FAIL_MESSAGE("ADS1115 constructor threw an unexpected exception "
                          "type when given a null I2C interface");
    }
}

TEST_CASE("[ADS1115].Initialization.add_bus_device_with_default_address",
          "If adding the device to the I2C bus fails during initialization, "
          "the constructor should throw") {

    using kiwi::i2c::ADS1115;
    std::shared_ptr<MockI2C> p_mock_i2c = std::make_shared<MockI2C>();
    std::unique_ptr<ADS1115> p_ads1115;

    p_mock_i2c->ExpectGetBusNumber([]() { return 1; });
    p_mock_i2c->ExpectAddBusDevice([](uint8_t actual) {
        TEST_ASSERT_EQUAL_UINT(ADS1115::k_DEFAULT_I2C_ADDRESS, actual);
        throw test_successful_halt();
        return ESP_FAIL;
    });
    p_mock_i2c->ExpectGetBusNumber([]() { return 2; });

    try {
        p_ads1115.reset(new ADS1115(p_mock_i2c));

    } catch (const test_successful_halt &) {
        // Test succeeded - the expected call to AddBusDevice was made, and the
        // constructor threw as expected. Now we just need to validate that
        // there were no unexpected calls to the mock I2C interface.
    } catch (...) {
        TEST_FAIL_MESSAGE("ADS1115 constructor threw an unexpected exception "
                          "type when AddBusDevice fails during initialization");
    }
}
