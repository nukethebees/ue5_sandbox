#pragma once

#include <ioj/duration.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <expected>
#include <string>
#include <vector>

namespace ioj::sim {
struct SimClock;
}

namespace ioj::levels {
using LevelCompilationErrors = std::vector<std::string>;

auto compile_timer_duration(Duration duration, sim::SimClock const& clock)
    -> std::expected<sim::SimTick, LevelCompilationErrors>;
[[nodiscard]] auto compile_level(LevelDefinition const& level, sim::LevelSimInitData& output)
    -> std::expected<void, LevelCompilationErrors>;
} // namespace ioj::levels
