#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct PackedValueSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> storage_type;
    std::vector<SchemaRange<PackedSegmentId>> segments;
    std::vector<std::optional<std::uint64_t>> invalid_value;
    std::vector<IdentifierId> export_specifier;
    std::vector<std::uint8_t> mutable_value;
    std::vector<std::optional<PackedByteOrder>> byte_order;
    std::vector<std::optional<PackedBitOrder>> bit_order;
};

}
