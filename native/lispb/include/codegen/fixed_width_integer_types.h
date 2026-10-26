#pragma once

#include <array>
#include <string_view>

namespace lispb::codegen {

struct FixedWidthIntegerType {
    std::string_view name;
    std::string_view native_name;
    int width;
    bool is_signed;
};

inline constexpr std::array fixed_width_integer_types{
    FixedWidthIntegerType{"int8", "std::int8_t", 8, true},
    FixedWidthIntegerType{"uint8", "std::uint8_t", 8, false},
    FixedWidthIntegerType{"int16", "std::int16_t", 16, true},
    FixedWidthIntegerType{"uint16", "std::uint16_t", 16, false},
    FixedWidthIntegerType{"int32", "std::int32_t", 32, true},
    FixedWidthIntegerType{"uint32", "std::uint32_t", 32, false},
    FixedWidthIntegerType{"int64", "std::int64_t", 64, true},
    FixedWidthIntegerType{"uint64", "std::uint64_t", 64, false},
};

}
