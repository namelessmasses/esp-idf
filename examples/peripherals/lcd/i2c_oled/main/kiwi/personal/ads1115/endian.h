#pragma once

#include <limits.h>
#include <stdint.h>

static inline uint16_t ads1115_endian_swap_16(uint16_t in) {
    constexpr size_t k_sz = sizeof(uint16_t);
    static_assert(k_sz == 2, "ads1115_endian_swap_16 requires 16-bit input");
    return (in << CHAR_BIT) | (in >> CHAR_BIT);
}