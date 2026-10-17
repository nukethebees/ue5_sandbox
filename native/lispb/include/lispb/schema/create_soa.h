#pragma once

#include <codegen/schema/soa_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateSoa {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::SoaSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
