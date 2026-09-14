#pragma once
#include <ioj/sim/level_sim.h>

namespace ioj::sim::tests {
struct SimulationFixture {
    LevelSimInitData data;
    ioj::sim::player::PlayerSpawnData player;
};
auto make_fixture() -> SimulationFixture;
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData;
auto make_player_spawn(SimulationFixture const& fixture, ioj::sim::Transform3d transform = {})
    -> ioj::sim::player::PlayerSpawnData;
auto add_capital_spawn(LevelSimInitData& data,
                       ioj::sim::Vector3f location,
                       ioj::sim::Team team,
                       std::int32_t target_spawn_index = -1,
                       float initial_spawn_delay = 0.f,
                       float spawn_cooldown = 60.f,
                       std::int32_t health = -1) -> std::int32_t;

}
