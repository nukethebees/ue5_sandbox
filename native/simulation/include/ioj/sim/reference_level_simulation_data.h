#pragma once

#include <ioj/sim/level_sim.h>

namespace ioj::sim {
struct ReferenceLevelSimulationData {
    LevelSimInitData data;
    player::PlayerSpawnData player;
};

[[nodiscard]] auto make_reference_level_simulation_data() -> ReferenceLevelSimulationData;
} // namespace ioj::sim
