#pragma once

#include <codegen/schema/child_occurrence_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/s7_field_options.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct ChildOccurrences {
    using Indexing = OneBasedIndex<ChildOccurrenceId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> ChildOccurrenceId;
    [[nodiscard]] auto clone(ChildOccurrenceId id) -> ChildOccurrenceId;
    [[nodiscard]] auto index(ChildOccurrenceId id) const -> std::uint32_t;
    void clear();

    Column<ChildOccurrenceKind> kind;
    Column<StringId> name;
    Column<TypeRefId> type;
    Column<std::optional<std::uint64_t>> count;
    Column<std::optional<std::uint32_t>> bits;
    Column<std::optional<StringId>> case_label;
    Column<std::optional<StringId>> tag;
    Column<std::optional<SemanticRelationId>> relationship;
    Column<std::optional<StringId>> initializer;
    Column<S7FieldOptions> s7;
    Column<std::optional<PackedIntegerValue>> default_value;
};

}
