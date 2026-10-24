#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct RecordSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<std::optional<StringId>> constraint_policy;
    std::vector<std::uint8_t> generate_settings;
    std::vector<std::uint8_t> generate_overrides;
    std::vector<std::optional<StringId>> s7_tag;
    std::vector<std::uint8_t> s7_positional;
    std::vector<SchemaRange<StringId>> s7_reader_includes;
    std::vector<StringId> name;
    std::vector<SchemaRange<RecordMemberId>> members;
    std::vector<StringId> export_specifier;
    std::vector<RecordComparison> comparison;
    std::vector<std::uint8_t> comparison_noexcept;
    std::vector<SchemaRange<FunctionSchemaId>> functions;
};

}
