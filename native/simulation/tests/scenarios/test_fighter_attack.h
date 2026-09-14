#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
void run_worldless_fighter_attack(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_obstacle_avoidance(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_capital_obstruction(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_clear_navigation(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_separation(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_dense_determinism(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_large_cluster(ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_hard_avoidance_authority(
    ioj::sim::tests::SimulationFixture const& config);
void run_worldless_fighter_navigation_frequency(ioj::sim::tests::SimulationFixture const& config);
}
