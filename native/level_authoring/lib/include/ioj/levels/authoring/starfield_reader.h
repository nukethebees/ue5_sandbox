#pragma once

#include <ioj/gfx/starfield_settings.h>
#include <ioj/levels/diagnostics.h>

#include <expected>
#include <filesystem>

namespace ioj::levels::authoring {
// Apply starfield sections in order to native defaults; ignore other settings sections.
[[nodiscard]] auto
    read_starfield_settings_file(std::filesystem::path const& path,
                                 std::filesystem::path const& script_library_root = {})
        -> std::expected<starfield::Settings, Diagnostics>;
}
