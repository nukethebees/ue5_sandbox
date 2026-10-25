#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct PackedValueSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::packed_value>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> storage_type;
    Column<SchemaRange<PackedSegmentId>> segments;
    Column<std::optional<std::uint64_t>> invalid_value;
    Column<StringId> export_specifier;
    Column<std::uint8_t> mutable_value;
    Column<std::optional<PackedByteOrder>> byte_order;
    Column<std::optional<PackedBitOrder>> bit_order;
};

}
