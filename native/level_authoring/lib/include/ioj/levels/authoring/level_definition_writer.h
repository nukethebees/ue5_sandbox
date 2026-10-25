#pragma once
#include <ioj/levels/level_definition.h>
#include <ioj/s7/ast.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
// Quote an evaluated value so rereading it does not execute its original source or imports.
[[nodiscard]] auto emit_evaluated_source(s7::Ast const& ast, s7::NodeIndex node) -> std::string;
// Preserve evaluated authored settings overrides; inherited settings remain inherited.
// Without authored source, require default settings to avoid discarding custom values.
// An explicitly empty source denotes a new scene with no authored settings overrides.
[[nodiscard]] auto
    emit_editor_level_source(LevelDefinition const& level,
                             std::optional<std::string_view> authored_source = std::nullopt,
                             std::filesystem::path const& script_library_root = {})
        -> std::expected<std::string, Diagnostics>;
}
