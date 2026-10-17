#pragma once

#include <codegen/schema/packed_field_schema.h>
#include <lispb/schema/integer_scalar_type.h>
#include <lispb/schema/type_definition.h>
#include <lispb/schema/type_id.h>
#include <lispb/schema/type_identity.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::schema {
struct TypeNode {
    TypeIdentity identity;
    std::string cpp_spelling;
    TypeDefinition definition;
    std::vector<TypeId> dependencies;
    std::vector<TypeId> users;
    std::optional<TypeIdentity> owning_declaration;
};

auto integer_domain(TypeNode const& node) -> IntegerScalarType const*;
auto packed_integer_domain(TypeNode const& node, codegen::PackedFieldSchema const& field)
    -> IntegerScalarType const*;
}
