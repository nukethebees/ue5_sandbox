#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <lispb/schema/indices.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
class TypeGraph;
}

namespace lispb::codegen::detail {
struct SettingsField {
    std::string path;
    std::string member;
};

auto settings_record(schema::TypeGraph const& graph, ModuleHandle module, TypeRefId reference)
    -> std::optional<schema::TypeId>;
auto settings_fields(schema::TypeGraph const& graph, ModuleHandle module, DeclarationHandle schema)
    -> std::vector<SettingsField>;
}
