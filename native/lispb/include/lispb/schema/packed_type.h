#pragma once

#include <codegen/schema/packed_order.h>
#include <lispb/schema/packed_segment.h>
#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {
struct PackedType {
    ResolvedTypeRef storage_type;
    std::vector<PackedSegment> segments;
    std::optional<std::uint64_t> invalid_raw_value;
    std::optional<codegen::PackedByteOrder> byte_order;
    codegen::PackedBitOrder bit_order{codegen::PackedBitOrder::least_significant_first};
    std::optional<std::uint64_t> default_raw_value{};
};
}
