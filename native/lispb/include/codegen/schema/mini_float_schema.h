#pragma once

#include <cstdint>
#include <string>

namespace codegen {

struct MiniFloatSchema {
    std::string name;
    std::uint32_t sign_bits{};
    std::uint32_t exponent_bits{};
    std::uint32_t significand_bits{};
    std::int32_t exponent_bias{};
};

} // namespace codegen
