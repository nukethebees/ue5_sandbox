#pragma once

#include <ioj/layout/schema_loader.hpp>

#include <functional>

namespace ioj::layout::detail {

using SchemaOpener =
    std::function<SchemaLoadResult(std::filesystem::path const&, std::string const&)>;

auto clone_lispb_schema_with_opener(lispb::schema::EditableSchemaDocument const& document,
                                    std::filesystem::path const& destination_project_path,
                                    std::string const& target_name,
                                    SchemaOpener const& opener) -> SchemaLoadResult;

} // namespace ioj::layout::detail
