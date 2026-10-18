#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/indices.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace lispb::schema {

struct CreateDeclarationEdits {
    std::vector<DeclarationId> declarations;
    std::vector<std::size_t> module_indices;
    std::vector<codegen::DeclarationHandle> versions;
    std::vector<std::optional<std::size_t>> insertion_indices;
};

} // namespace lispb::schema
