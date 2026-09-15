#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class LaserLifecycleScenario : std::uint8_t {
    Hit,
    SimultaneousLethalHits,
    Miss,
    WorldBlocker
};

void run_worldless_laser_lifecycle(tests::SimulationFixture const& config,
                                   LaserLifecycleScenario scenario);
}
