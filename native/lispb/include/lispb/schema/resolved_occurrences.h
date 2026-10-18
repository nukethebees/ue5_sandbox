#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/physical_type_form.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct ResolvedOccurrences {
    [[nodiscard]] auto append() -> ResolvedOccurrenceId;
    // IDs must name a row in this store; zero denotes absence at optional uses.
    [[nodiscard]] auto index(ResolvedOccurrenceId id) const -> std::uint32_t;
    [[nodiscard]] auto contains_value(ResolvedOccurrenceId id) const -> bool;

    std::vector<TypeId> type;
    std::vector<codegen::CppTypeId> cpp_type;
    std::vector<codegen::PhysicalTypeForm> physical_form;
    std::vector<StringId> object_spelling;
    std::vector<std::uint8_t> names_semantic_type;
    std::vector<std::uint8_t> cv_qualified;
};

} // namespace lispb::schema
