#pragma once

#include <limits.h>
#include <stdint.h>

#if defined(__cplusplus)
#include <cassert>
#else
#include <assert.h>
#endif

static inline uint16_t ads1115_endian_swap_16(uint16_t in) {
#if defined(__cplusplus)
    constexpr size_t k_sz = sizeof(uint16_t);
    static_assert(k_sz == 2, "ads1115_endian_swap_16 requires 16-bit input");
#else
    assert(sizeof(uint16_t) == 2 && "ads1115_endian_swap_16 requires 16-bit input");
#endif
    return (in << CHAR_BIT) | (in >> CHAR_BIT);
}
