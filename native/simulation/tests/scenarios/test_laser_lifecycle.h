#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
enum class ELaserLifecycleScenario : std::uint8_t {
    Hit,
    SimultaneousLethalHits,
    Miss,
    WorldBlocker
};

void run_worldless_laser_lifecycle(ml::simulation_tests::SimulationFixture const& config,
                                   ELaserLifecycleScenario scenario);
}
