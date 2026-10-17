#pragma once

#include <lispb/schema/declaration_id.h>

#include <cstddef>
#include <optional>

namespace lispb::schema {
struct MoveDeclaration {
    DeclarationId declaration;
    std::size_t module_index{};
    std::optional<std::size_t> insertion_index;
    bool restore_source_ownership{};
};
}
