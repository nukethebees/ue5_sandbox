#pragma once

#include <cstdint>

namespace lispb::schema {
struct MiniFloatType {
    std::uint32_t sign_bits{};
    std::uint32_t exponent_bits{};
    std::uint32_t significand_bits{};
    std::int32_t exponent_bias{};
};
}
