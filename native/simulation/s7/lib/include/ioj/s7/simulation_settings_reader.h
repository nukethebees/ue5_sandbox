#pragma once

#include <ioj/settings/settings.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <filesystem>

namespace ioj::s7 {
using SimulationSettingsReadResult = std::expected<sim::SimulationSettings, levels::Diagnostics>;

[[nodiscard]] auto read_simulation_settings(std::filesystem::path const& path,
                                            std::filesystem::path const& library_root = {})
    -> SimulationSettingsReadResult;
}
