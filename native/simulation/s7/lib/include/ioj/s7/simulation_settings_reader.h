#pragma once

#include <ioj/settings/settings.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <filesystem>

namespace ioj::levels {
class LevelDefinition;
}

namespace ioj::s7 {
using SimulationSettingsReadResult = std::expected<sim::SimulationSettings, levels::Diagnostics>;

// Decode supplied records into default-constructed native settings, then apply level overrides.
[[nodiscard]] auto resolve_simulation_settings(settings::Settings const& source,
                                               levels::LevelDefinition const* level = nullptr)
    -> SimulationSettingsReadResult;

[[nodiscard]] auto read_simulation_settings(std::filesystem::path const& path,
                                            std::filesystem::path const& library_root = {},
                                            levels::LevelDefinition const* level = nullptr)
    -> SimulationSettingsReadResult;
}
