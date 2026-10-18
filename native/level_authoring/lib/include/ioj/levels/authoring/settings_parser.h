#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/s7/ast.h>
#include <ioj/settings/settings.h>

namespace ioj::levels::authoring {
using SettingsReadResult = std::expected<settings::Settings, Diagnostics>;

[[nodiscard]] auto parse_settings(s7::Ast const& ast, s7::NodeIndex root) -> SettingsReadResult;
}
