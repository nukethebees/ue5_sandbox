#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class EntityLedgerScenario : std::uint8_t { TeamCounts, OnePlayerKill, TwoPlayerKills };

void run_worldless_entity_ledger_scenario(tests::SimulationFixture const& config,
                                          EntityLedgerScenario scenario);
}
