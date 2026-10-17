#pragma once

#include <codegen/schema/tagged_union_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateTaggedUnion {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::TaggedUnionSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
