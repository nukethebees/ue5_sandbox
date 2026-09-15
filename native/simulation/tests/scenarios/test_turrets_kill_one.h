#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class TurretCombatScenario : std::uint8_t { KillEnemy, ZeroDamage };

void run_worldless_turret_combat(tests::SimulationFixture const& config,
                                 TurretCombatScenario scenario);
void run_worldless_turret_line_of_sight_blocking(tests::SimulationFixture const& config);
void run_worldless_turret_search_requires_line_of_sight(tests::SimulationFixture const& config);
}
