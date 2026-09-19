#pragma once

#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/model.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace ioj::layout {

struct CatalogLoadResult {
    SchemaCatalog catalog;
    std::vector<Diagnostic> diagnostics;
    bool loaded{};
};

auto load_lispb_catalog(std::filesystem::path const& project_path, std::string const& target_name)
    -> CatalogLoadResult;

} // namespace ioj::layout
