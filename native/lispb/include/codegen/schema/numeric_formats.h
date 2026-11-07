#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/min_max.h>

#include <codegen/schema/fixed_point_form.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {
using BitWidth = ioj::Bounded<std::uint32_t, 1, 64>;
using EnumBitWidth = ioj::Bounded<std::uint8_t, 1, 64>;
using SignWidth = ioj::Bounded<std::uint32_t, 0, 1>;
using ExponentWidth = ioj::Bounded<std::uint32_t, 2, 15>;
using SignificandWidth = ioj::Bounded<std::uint32_t, 0, 62>;
using ExponentBias = ioj::Bounded<std::int32_t, -32768, 32767>;

struct MiniFloatFields {
    SignWidth sign_bits;
    ExponentWidth exponent_bits;
    SignificandWidth significand_bits;
    ExponentBias exponent_bias;
};
inline constexpr auto mini_float_policy{ioj::ConstrainedValuePolicy{
    [](MiniFloatFields const& value) constexpr noexcept {
        return value.sign_bits + value.exponent_bits + value.significand_bits <= 64;
    },
    "Mini-float total width must not exceed 64 bits"}};
using MiniFloatFormat = ioj::ConstrainedValue<MiniFloatFields, mini_float_policy>;

struct FixedPointFields {
    BitWidth total_bits;
    FixedPointForm form{FixedPointForm::binary_point};
    bool signedness{};
    std::uint32_t fractional_bits{};
    ioj::MinMax<ioj::Finite<double>> values{};
    StringId minimum_spelling{};
    StringId maximum_spelling{};
};
inline constexpr auto fixed_point_policy{ioj::ConstrainedValuePolicy{
    [](FixedPointFields const& value) constexpr noexcept {
        if (value.form == FixedPointForm::binary_point) {
            return value.fractional_bits <= value.total_bits - (value.signedness ? 1U : 0U);
        }
        return value.form == FixedPointForm::range && !value.signedness &&
               value.fractional_bits == 0 && value.values.max > value.values.min;
    },
    "Invalid fixed-point format"}};
using FixedPointFormat = ioj::ConstrainedValue<FixedPointFields, fixed_point_policy>;
}
