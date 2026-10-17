#pragma once

#include <codegen/schema/fixed_point_schema.h>
#include <codegen/schema/packed_integer_value.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {
struct FixedPointType {
    bool signedness{};
    std::uint32_t total_bits{};
    std::uint32_t fractional_bits{};
    codegen::FixedPointRounding rounding{codegen::FixedPointRounding::nearest_even};
    std::optional<codegen::PackedIntegerValue> minimum_raw_value;
    std::optional<codegen::PackedIntegerValue> maximum_raw_value;
};
}
