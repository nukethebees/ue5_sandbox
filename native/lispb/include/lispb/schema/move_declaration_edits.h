#pragma once

#include <lispb/schema/indices.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace lispb::schema {

struct MoveDeclarationEdits {
    std::vector<DeclarationId> declarations;
    std::vector<std::size_t> module_indices;
    std::vector<std::optional<std::size_t>> insertion_indices;
    std::vector<bool> restore_source_ownership;
};

} // namespace lispb::schema
