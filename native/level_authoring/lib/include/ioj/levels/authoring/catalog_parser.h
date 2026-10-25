#pragma once
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/s7/ast.h>

namespace ioj::levels::authoring {
// Borrow AST storage; parse each definition from its own root node.
// Apply base settings before validating each included level.
[[nodiscard]] auto parse_catalog(s7::Ast const& ast,
                                 std::filesystem::path const& source_root,
                                 settings::Settings const& base = {})
    -> std::expected<DefinitionCatalog, Diagnostics>;
}
