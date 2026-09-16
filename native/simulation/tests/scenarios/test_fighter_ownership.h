#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {

enum class FighterOwnershipScenario : std::uint8_t { KillFightersOnly, KillCapital, All };

void run_worldless_simultaneous_capital_reassignment(tests::SimulationFixture const& config);
void run_worldless_fighter_ownership(tests::SimulationFixture const& config,
                                     FighterOwnershipScenario scenario);
}
