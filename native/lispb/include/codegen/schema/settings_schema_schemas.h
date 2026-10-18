#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {
struct SettingsSchemaSchemas {
    using RowIndex = decltype(DeclarationHandle{}.index());

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> RowIndex;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<IdentifierId> target;
    std::vector<StringId> target_header;
    std::vector<SchemaRange<StringId>> paths;
    std::vector<SchemaRange<StringId>> members;
};

[[nodiscard]] auto settings_schema_function_stem(std::string_view name) -> std::string;
[[nodiscard]] auto settings_schema_field_name(std::string_view path) -> std::string;
}
