#pragma once

#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/type_graph.h>

#include <filesystem>
#include <string>
#include <vector>

namespace ioj::layout {

struct SchemaLoadResult {
    lispb::schema::TypeGraph types;
    std::vector<Diagnostic> diagnostics;
    bool loaded{};
};

auto load_lispb_schema(std::filesystem::path const& project_path, std::string const& target_name)
    -> SchemaLoadResult;

} // namespace ioj::layout
