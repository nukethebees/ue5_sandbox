#pragma once

#include <codegen/schema/union_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateUnion {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::UnionSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
