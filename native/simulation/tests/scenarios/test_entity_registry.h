#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
enum class EEntityRegistryScenario : std::uint8_t { TeamCounts, OnePlayerKill, TwoPlayerKills };

void run_worldless_entity_registry_scenario(ml::simulation_tests::SimulationFixture const& config,
                                            EEntityRegistryScenario scenario);
}
