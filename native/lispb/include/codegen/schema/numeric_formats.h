#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/min_max.h>

#include <codegen/schema/fixed_point_form.h>
#include <lispb/indices.h>

#include <cassert>
#include <cstdint>
#include <variant>

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

struct BinaryPointFields {
    BitWidth total_bits;
    bool signedness;
    std::uint32_t fractional_bits;
};
inline constexpr auto binary_point_policy{ioj::ConstrainedValuePolicy{
    [](BinaryPointFields const& value) constexpr noexcept {
        return value.fractional_bits <= value.total_bits - (value.signedness ? 1U : 0U);
    },
    "Fixed-point fractional width must fit the total width, leaving a sign bit when signed"}};
using BinaryPointFormat = ioj::ConstrainedValue<BinaryPointFields, binary_point_policy>;

struct RangePointFields {
    BitWidth total_bits;
    ioj::MinMax<ioj::Finite<double>> values;
    StringId minimum_spelling;
    StringId maximum_spelling;
};
inline constexpr auto range_point_policy{
    ioj::ConstrainedValuePolicy{[](RangePointFields const& value) constexpr noexcept {
                                    return value.values.max > value.values.min;
                                },
                                "Fixed-point maximum must be greater than minimum"}};
using RangePointFormat = ioj::ConstrainedValue<RangePointFields, range_point_policy>;

class FixedPointFormat {
  public:
    constexpr FixedPointFormat(BinaryPointFormat const value)
        : value_{value} {}
    constexpr FixedPointFormat(RangePointFormat const value)
        : value_{value} {}

    [[nodiscard]] auto form() const noexcept -> FixedPointForm {
        return std::holds_alternative<BinaryPointFormat>(value_) ? FixedPointForm::binary_point
                                                                 : FixedPointForm::range;
    }
    [[nodiscard]] auto total_bits() const noexcept -> BitWidth {
        return std::visit([](auto const& value) { return value.ref().total_bits; }, value_);
    }
    [[nodiscard]] auto signedness() const noexcept -> bool {
        auto const* binary{std::get_if<BinaryPointFormat>(&value_)};
        return binary && binary->ref().signedness;
    }
    [[nodiscard]] auto binary() const noexcept -> BinaryPointFields const& {
        assert(form() == FixedPointForm::binary_point);
        return std::get<BinaryPointFormat>(value_).ref();
    }
    [[nodiscard]] auto range() const noexcept -> RangePointFields const& {
        assert(form() == FixedPointForm::range);
        return std::get<RangePointFormat>(value_).ref();
    }
    [[nodiscard]] auto value_range() const noexcept -> ioj::MinMax<double> {
        auto const& values{range().values};
        return {values.min.get(), values.max.get()};
    }

  private:
    std::variant<BinaryPointFormat, RangePointFormat> value_;
};
}
