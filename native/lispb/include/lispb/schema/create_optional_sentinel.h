#pragma once

#include <codegen/schema/optional_sentinel_schema.h>
#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct CreateOptionalSentinel {
    DeclarationId declaration;
    std::size_t module_index{};
    codegen::OptionalSentinelSchema schema;
    std::optional<std::size_t> insertion_index;
};
}
