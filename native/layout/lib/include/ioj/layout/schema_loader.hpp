#pragma once

#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/editable_document.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct SchemaLoadResult {
    std::optional<lispb::schema::EditableSchemaDocument> document;
    std::vector<Diagnostic> diagnostics;
    std::filesystem::path project_path;
    std::string target_name;
    bool loaded{};
};

auto load_lispb_schema(std::filesystem::path const& project_path, std::string const& target_name)
    -> SchemaLoadResult;
auto clone_lispb_schema(lispb::schema::EditableSchemaDocument const& document,
                        std::filesystem::path const& destination_project_path,
                        std::string const& target_name) -> SchemaLoadResult;

} // namespace ioj::layout
