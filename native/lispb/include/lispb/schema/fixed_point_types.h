#pragma once

#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct FixedPointTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<std::uint8_t> signedness;
    std::vector<std::uint32_t> total_bits;
    std::vector<std::uint32_t> fractional_bits;
    std::vector<codegen::FixedPointRounding> rounding;
    std::vector<std::optional<codegen::PackedIntegerValue>> minimum_raw_value;
    std::vector<std::optional<codegen::PackedIntegerValue>> maximum_raw_value;
};

} // namespace lispb::schema
