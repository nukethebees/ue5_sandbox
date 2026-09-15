#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
void run_worldless_fighter_attack(tests::SimulationFixture const& config);
void run_worldless_fighter_obstacle_avoidance(tests::SimulationFixture const& config);
void run_worldless_fighter_capital_obstruction(tests::SimulationFixture const& config);
void run_worldless_fighter_clear_navigation(tests::SimulationFixture const& config);
void run_worldless_fighter_separation(tests::SimulationFixture const& config);
void run_worldless_fighter_dense_determinism(tests::SimulationFixture const& config);
void run_worldless_fighter_large_cluster(tests::SimulationFixture const& config);
void run_worldless_fighter_hard_avoidance_authority(tests::SimulationFixture const& config);
void run_worldless_fighter_navigation_frequency(tests::SimulationFixture const& config);
}
