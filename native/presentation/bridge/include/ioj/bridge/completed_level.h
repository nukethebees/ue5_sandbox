#pragma once

#include <ioj/profiles/level_score.h>

namespace ioj::sim {
struct LevelSim;
struct LevelResult;
}
namespace ioj::bridge {
auto completed_level(sim::LevelSim const& level, sim::LevelResult const& result)
    -> profiles::CompletedLevel;
}
