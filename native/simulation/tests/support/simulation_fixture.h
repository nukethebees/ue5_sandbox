#pragma once
#include <sandbox/simulation/simulation/LevelSimulation.h>

namespace ml::simulation_tests {
struct SimulationFixture {
    FLevelSimulationInitData data;
    ml::test_space_ship::FPlayerSpawnData player;
};
auto make_fixture() -> SimulationFixture;
auto make_simulation_data(SimulationFixture const& fixture) -> FLevelSimulationInitData;
auto make_player_spawn(SimulationFixture const& fixture, ml::simulation::Transform3d transform = {})
    -> ml::test_space_ship::FPlayerSpawnData;
auto add_capital_spawn(FLevelSimulationInitData& data,
                       ml::simulation::Vector3f location,
                       ml::simulation::Team team,
                       std::int32_t target_spawn_index = -1,
                       float initial_spawn_delay = 0.f,
                       float spawn_cooldown = 60.f,
                       std::int32_t health = -1) -> std::int32_t;

}
