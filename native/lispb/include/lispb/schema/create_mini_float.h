#pragma once

#include <codegen/schema/mini_float_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateMiniFloat {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::MiniFloatSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
