#pragma once

#include <codegen/schema/module_schema.h>
#include <lispb/schema/declaration_info.h>
#include <lispb/schema/source_range.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace lispb::schema {
struct RestoreModule {
    std::size_t module_index{};
    codegen::ModuleSchema schema;
    std::optional<SourceRange> source;
    std::optional<std::size_t> pending_source_file_index;
    std::vector<DeclarationInfo> declarations;
};
}
