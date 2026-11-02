#pragma once

#include <cstdint>
#include <limits>

namespace lispb::codegen {

struct IntegerType {
    auto maximum() const -> std::uint64_t {
        auto const value_bits{bits - static_cast<unsigned>(is_signed)};
        return value_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                : (std::uint64_t{1} << value_bits) - 1;
    }
    auto contains(std::uint64_t magnitude, bool negative) const -> bool {
        return bits != 0 &&
               (negative ? is_signed && magnitude <= maximum() + 1 : magnitude <= maximum());
    }
    auto promoted() const -> IntegerType { return bits < 32 ? IntegerType{32, true} : *this; }
    auto operator==(IntegerType const&) const -> bool = default;

    unsigned bits{};
    bool is_signed{};
};

struct IntegerConstant {
    std::uint64_t magnitude;
    bool negative;
    IntegerType type;
};

} // namespace lispb::codegen
