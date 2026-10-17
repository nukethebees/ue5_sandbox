#pragma once

#include <codegen/schema/packed_value_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreatePackedValue {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::PackedValueSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
