#pragma once

#include <codegen/schema/record_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateRecord {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::RecordSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
