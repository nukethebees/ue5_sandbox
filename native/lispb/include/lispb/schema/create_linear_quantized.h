#pragma once

#include <codegen/schema/linear_quantized_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateLinearQuantized {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::LinearQuantizedSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
