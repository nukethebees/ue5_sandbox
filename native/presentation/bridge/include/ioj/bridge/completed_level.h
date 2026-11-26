#pragma once

#include <ioj/profiles/level_score.h>

#include <optional>

namespace ioj::sim {
struct LevelSim;
}
namespace ioj::bridge {
auto completed_level(sim::LevelSim const& level) -> std::optional<profiles::CompletedLevel>;
}
