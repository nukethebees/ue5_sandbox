#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <optional>

namespace lispb::schema {
class TypeGraph;

auto integer_domain(TypeGraph const& graph, TypeId type) -> std::optional<TypeDefinitionId>;
}
