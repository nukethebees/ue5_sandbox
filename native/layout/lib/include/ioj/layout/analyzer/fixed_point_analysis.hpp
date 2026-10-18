#pragma once

#include <ioj/layout/diagnostic.hpp>

#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct FixedPointAnalysis {
    lispb::schema::TypeId type{};
    bool signedness{};
    std::uint32_t total_bits{};
    std::uint32_t fractional_bits{};
    std::uint32_t whole_bits{};
    lispb::codegen::PackedIntegerValue minimum_raw_value{};
    lispb::codegen::PackedIntegerValue maximum_raw_value{};
    lispb::codegen::PackedIntegerValue minimum_allowed_raw_value{};
    lispb::codegen::PackedIntegerValue maximum_allowed_raw_value{};
    long double scale{};
    long double resolution{};
    long double minimum_value{};
    long double maximum_value{};
    long double minimum_allowed_value{};
    long double maximum_allowed_value{};
    long double maximum_rounding_error{};
    lispb::codegen::FixedPointRounding rounding{lispb::codegen::FixedPointRounding::nearest_even};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits{};
    std::vector<Diagnostic> diagnostics{};
};

}
