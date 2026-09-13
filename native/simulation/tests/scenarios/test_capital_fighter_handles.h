#pragma once
#include "../support/simulation_test_support.h"

namespace ml {

enum class ECapitalFighterHandlesScenario : std::uint8_t { KillFightersOnly, KillCapital, All };

void run_worldless_simultaneous_capital_reassignment(
    ml::simulation_tests::SimulationFixture const& config);
void run_worldless_capital_fighter_handles(ml::simulation_tests::SimulationFixture const& config,
                                           ECapitalFighterHandlesScenario scenario);
}
