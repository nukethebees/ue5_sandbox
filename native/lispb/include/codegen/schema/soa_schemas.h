#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct SoaSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<std::optional<StringId>> view_name;
    std::vector<std::optional<StringId>> const_view_name;
    std::vector<SchemaRange<SoaMemberId>> members;
    std::vector<StringId> export_specifier;
    std::vector<SchemaRange<FunctionSchemaId>> functions;
    std::vector<SchemaRange<FunctionSchemaId>> mutable_view_functions;
    std::vector<SchemaRange<StringId>> using_declarations;
    std::vector<std::optional<TypeRefId>> equivalent_type;
    std::vector<std::optional<StringId>> equivalent_constructor;
    std::vector<std::uint8_t> layout_only;
    std::vector<std::optional<StringId>> field_mask_name;
    std::vector<std::optional<StringId>> field_enum_name;
    std::vector<SchemaRange<FunctionSchemaId>> const_view_functions;
};

}
