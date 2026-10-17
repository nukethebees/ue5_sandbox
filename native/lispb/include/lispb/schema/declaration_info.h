#pragma once

#include <lispb/schema/declaration_id.h>
#include <lispb/schema/source_range.h>
#include <lispb/schema/type_identity.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct DeclarationInfo {
    DeclarationId id;
    TypeIdentity identity;
    std::size_t module_index{};
    std::size_t declaration_index{};
    std::optional<SourceRange> source;
};
}
