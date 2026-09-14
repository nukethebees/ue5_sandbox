#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class TurretCombatScenario : std::uint8_t { KillEnemy, ZeroDamage };

void run_worldless_turret_combat(ioj::sim::tests::SimulationFixture const& config,
                                 TurretCombatScenario scenario);
void run_worldless_turret_line_of_sight_blocking(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_turret_search_requires_line_of_sight(
    ioj::sim::tests::SimulationFixture const& config);
}
