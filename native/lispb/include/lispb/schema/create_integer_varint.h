#pragma once

#include <codegen/schema/integer_varint_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateIntegerVarint {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::IntegerVarintSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
