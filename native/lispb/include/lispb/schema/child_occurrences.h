#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {

struct ChildOccurrences {
    using Index = OneBasedIndex<ChildOccurrenceId>;

    [[nodiscard]] auto append() -> ChildOccurrenceId;
    [[nodiscard]] auto index(ChildOccurrenceId id) const -> std::uint32_t;

    IndexedColumn<codegen::ChildOccurrenceId, Index> source;
    IndexedColumn<StringId, Index> name;
    IndexedColumn<ResolvedOccurrenceId, Index> semantic_type;
    IndexedColumn<std::optional<std::uint64_t>, Index> count;
    IndexedColumn<RelationshipId, Index> relationship;
    IndexedColumn<std::optional<StringId>, Index> initializer;
    IndexedColumn<std::optional<std::uint32_t>, Index> bits;
    IndexedColumn<std::optional<StringId>, Index> case_label;
    IndexedColumn<std::optional<StringId>, Index> tag;
    IndexedColumn<std::optional<codegen::PackedIntegerValue>, Index> default_value;
    IndexedColumn<std::uint8_t, Index> reserved;
};

}
