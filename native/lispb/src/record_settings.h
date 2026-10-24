#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <lispb/schema/indices.h>

#include <optional>

namespace lispb::schema {
class TypeGraph;
}

namespace lispb::codegen::detail {
auto settings_record(schema::TypeGraph const& graph, ModuleHandle module, TypeRefId reference)
    -> std::optional<schema::TypeId>;
}
