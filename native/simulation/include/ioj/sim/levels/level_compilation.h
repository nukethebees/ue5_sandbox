#pragma once

#include <ioj/duration.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <expected>
#include <string>
#include <vector>

namespace ioj::levels {
using LevelCompilationErrors = std::vector<std::string>;

auto compile_timer_duration(Duration duration, Frequency tick_rate)
    -> std::expected<sim::TimerTicks, LevelCompilationErrors>;
[[nodiscard]] auto compile_level(LevelDefinition const& level, sim::LevelSimInitData& output)
    -> std::expected<void, LevelCompilationErrors>;
} // namespace ioj::levels
