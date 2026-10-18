#pragma once

#include <ioj/levels/level_definition.h>

namespace ioj::levels::authoring {
// Check editor capabilities, not semantic validity. A valid authored level may use
// runtime features the scene editor cannot represent.
[[nodiscard]] auto check_editor_support(LevelDefinition const& level)
    -> std::expected<void, Diagnostics>;
}
