#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {

enum class FighterHandlesScenario : std::uint8_t { KillFightersOnly, KillCapital, All };

void run_worldless_simultaneous_capital_reassignment(
    ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_handles(ioj::sim::tests::SimulationFixture const& config,
                                   FighterHandlesScenario scenario);
}
