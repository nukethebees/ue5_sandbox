#pragma once
#include <ioj/sim/reference_level_simulation_data.h>

namespace ioj::sim::tests {
using SimulationFixture = ReferenceLevelSimulationData;
inline auto make_fixture() -> SimulationFixture {
    return make_reference_level_simulation_data();
}
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData;
auto make_player_spawn(SimulationFixture const& fixture, Transform3d transform = {})
    -> player::PlayerSpawnData;
auto add_player_spawn(LevelSimInitData& data, player::PlayerSpawnData spawn) -> std::int32_t;
auto add_capital_spawn(LevelSimInitData& data,
                       Vector3f location,
                       Team team,
                       std::int32_t target_entity_index = -1,
                       float initial_spawn_delay = 0.f,
                       float spawn_cooldown = 60.f,
                       std::int32_t health = -1) -> std::int32_t;
auto add_turret_spawn(LevelSimInitData& data,
                      Vector3f location,
                      Rotator3f rotation,
                      Team team,
                      std::int32_t health = -1,
                      std::int32_t laser_damage = -1) -> std::int32_t;
auto add_spinner_spawn(LevelSimInitData& data,
                       Vector3f location,
                       float yaw,
                       std::int32_t initial_fire_point_index) -> std::int32_t;

}
