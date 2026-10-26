#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/physical_type_form.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>

namespace lispb::schema {

struct ResolvedOccurrences {
    using Index = OneBasedIndex<ResolvedOccurrenceId>;

    [[nodiscard]] auto append() -> ResolvedOccurrenceId;
    // IDs must name a row in this store; zero denotes absence at optional uses.
    [[nodiscard]] auto index(ResolvedOccurrenceId id) const -> std::uint32_t;
    [[nodiscard]] auto contains_value(ResolvedOccurrenceId id) const -> bool;

    IndexedColumn<TypeId, Index> type;
    IndexedColumn<codegen::CppTypeId, Index> cpp_type;
    IndexedColumn<codegen::PhysicalTypeForm, Index> physical_form;
    IndexedColumn<StringId, Index> object_spelling;
    IndexedColumn<std::uint8_t, Index> names_semantic_type;
    IndexedColumn<std::uint8_t, Index> cv_qualified;
};

} // namespace lispb::schema
