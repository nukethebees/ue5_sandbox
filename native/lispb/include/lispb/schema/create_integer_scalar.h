#pragma once

#include <codegen/schema/integer_scalar_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateIntegerScalar {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::IntegerScalarSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
