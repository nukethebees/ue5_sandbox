#pragma once

#include <codegen/schema/declaration_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateDeclaration {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::DeclarationSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
