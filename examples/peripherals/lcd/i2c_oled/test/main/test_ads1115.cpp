#include "unity.h"

#include "ads1115.h"

TEST_CASE("ads1115 encode address register pointer", "[ads1115]") {

    ads1115_address_pointer_register_t host_reg;
    ads1115_address_pointer_register_t i2c_reg;

    host_reg.P = ADS1115_REG_CONVERSION;
    ads1115_encode_address_register_pointer(&host_reg, &i2c_reg);

    // Bits[7:2] should be 0, bits[1:0] should be the register address.
    TEST_ASSERT_EQUAL_UINT8(ADS1115_REG_CONVERSION, i2c_reg.P);
}
