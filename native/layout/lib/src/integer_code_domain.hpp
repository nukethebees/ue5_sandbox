#pragma once

#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>

#include <codegen/schema/packed_integer_value.h>

#include <cstdint>
#include <optional>

namespace ioj::layout::analysis_detail {

auto maximum_unsigned_value(std::uint32_t const bits) -> std::optional<std::uint64_t>;

auto minimum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t>;

auto maximum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t>;

auto minimum_bits_for_code_count(ExactCodeCount const count) -> std::uint32_t;

auto packed_range_count(lispb::codegen::PackedIntegerValue const minimum,
                        lispb::codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t>;

auto packed_range_span(lispb::codegen::PackedIntegerValue const minimum,
                       lispb::codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t>;

auto packed_integer_as_long_double(lispb::codegen::PackedIntegerValue const value) -> long double;

auto exact_code_count_delta(ExactCodeCount const baseline, ExactCodeCount const variant)
    -> NumericDelta;

}
