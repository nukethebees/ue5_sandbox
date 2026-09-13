#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
void run_worldless_fighter_attack(ml::simulation_tests::SimulationFixture const& config);
void
    run_worldless_fighter_obstacle_avoidance(ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_capital_obstruction(
    ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_clear_navigation(ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_separation(ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_dense_determinism(ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_large_cluster(ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_hard_avoidance_authority(
    ml::simulation_tests::SimulationFixture const& config);
void run_worldless_fighter_navigation_frequency(
    ml::simulation_tests::SimulationFixture const& config);
}
