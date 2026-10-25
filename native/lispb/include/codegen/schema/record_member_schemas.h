#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/s7_field_options.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct RecordMemberSchemas {
    using Indexing = OneBasedIndex<RecordMemberId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> RecordMemberId;
    [[nodiscard]] auto clone(RecordMemberId id) -> RecordMemberId;
    [[nodiscard]] auto index(RecordMemberId id) const -> std::uint32_t;
    void clear();

    Column<S7FieldOptions> s7;
    Column<StringId> name;
    Column<TypeRefId> type;
    Column<std::optional<std::uint64_t>> count;
    Column<std::optional<SemanticRelationId>> relationship;
    Column<std::optional<StringId>> initializer;
};

}
