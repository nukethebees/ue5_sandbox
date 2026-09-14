#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class EntityRegistryScenario : std::uint8_t { TeamCounts, OnePlayerKill, TwoPlayerKills };

void run_worldless_entity_registry_scenario(ioj::sim::tests::SimulationFixture const& config,
                                            EntityRegistryScenario scenario);
}
