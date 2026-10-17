#pragma once

#include <codegen/schema/optional_presence_bit_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateOptionalPresenceBit {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::OptionalPresenceBitSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
