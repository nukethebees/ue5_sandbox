#pragma once
#include <ioj/levels/authoring/editor_level.h>

#include <string>

namespace ioj::levels::authoring {
[[nodiscard]] auto emit_editor_level_source(EditorLevel const& level) -> std::string;
}
