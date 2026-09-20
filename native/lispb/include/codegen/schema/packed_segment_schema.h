#pragma once

#include <codegen/schema/packed_field_schema.h>

#include <optional>
#include <string>
#include <variant>

namespace codegen {

struct PackedReservedBitsSchema {
    std::string name;
    int bits;
};

using PackedSegmentSchema = std::variant<PackedFieldSchema, PackedReservedBitsSchema>;

inline auto packed_segment_name(PackedSegmentSchema const& segment) -> std::string const& {
    return std::visit([](auto const& value) -> std::string const& { return value.name; }, segment);
}

inline auto packed_segment_bits(PackedSegmentSchema const& segment) -> std::optional<int> {
    return std::visit([](auto const& value) -> std::optional<int> { return value.bits; }, segment);
}

} // namespace codegen
