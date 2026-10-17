#pragma once

#include <codegen/schema/enum_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateEnum {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::EnumSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
