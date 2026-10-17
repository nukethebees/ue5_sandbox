#pragma once

#include <codegen/schema/fixed_point_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateFixedPoint {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::FixedPointSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
