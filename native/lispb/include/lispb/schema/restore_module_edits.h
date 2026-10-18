#pragma once

#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/source_range.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct RestoreModuleEdits {
    std::vector<std::size_t> module_indices;
    std::vector<codegen::ModuleHandle> versions;
    std::vector<std::optional<SourceRange>> sources;
    std::vector<std::optional<codegen::sexpr::SourceFileId>> pending_source_files;
    std::vector<std::uint32_t> declaration_offsets;
    std::vector<std::uint32_t> declaration_counts;
    std::vector<DeclarationId> declarations;
};

} // namespace lispb::schema
