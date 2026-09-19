#pragma once

#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/model.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace ioj::layout {

struct TypeRepresentation {
    std::string spelling;
    std::string represented_by;
};

struct CatalogLoadResult {
    SchemaCatalog catalog;
    std::vector<TypeRepresentation> type_representations;
    std::vector<Diagnostic> diagnostics;
    bool loaded{};
};

auto load_lispb_catalog(std::filesystem::path const& project_path, std::string const& target_name)
    -> CatalogLoadResult;

} // namespace ioj::layout
