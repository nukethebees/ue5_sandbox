#pragma once

#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct PackedReservedBitsSchemas {
    using Indexing = TaggedIndex<PackedSegmentId, PackedSegmentKind::reserved>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto clone(PackedSegmentId id) -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<int> bits;
};

}
