#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
enum class ETurretCombatScenario : std::uint8_t { KillEnemy, ZeroDamage };

void run_worldless_turret_combat(ml::simulation_tests::SimulationFixture const& config,
                                 ETurretCombatScenario scenario);
void run_worldless_turret_line_of_sight_blocking(
    ml::simulation_tests::SimulationFixture const& config);
void run_worldless_turret_search_requires_line_of_sight(
    ml::simulation_tests::SimulationFixture const& config);
}
