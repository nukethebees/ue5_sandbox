#pragma once
#include <ioj/sim/levels/reference_level_simulation_data.h>

namespace ioj::sim::tests {
using SimulationFixture = ReferenceLevelSimulationData;
inline auto make_fixture() -> SimulationFixture {
    auto fixture{make_reference_level_simulation_data()};
    fixture.data.lasers.n_preallocated_instances = 256;
    fixture.data.grid_geometry.dimensions = {64, 64, 5};
    fixture.data.grid_geometry.cell_dimensions = {31250.f, 31250.f, 20000.f};
    fixture.data.frame_memory_capacity_bytes = 4 * 1024 * 1024;
    return fixture;
}
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData;
auto make_player_spawn(SimulationFixture const& fixture, Transform3d transform = {})
    -> player::PlayerSpawnData;
auto add_player_spawn(LevelSimInitData& data, player::PlayerSpawnData spawn) -> std::uint32_t;
auto add_capital_spawn(LevelSimInitData& data,
                       Vector3f location,
                       Team team,
                       std::uint32_t target_entity_index = invalid_level_entity_index,
                       float initial_spawn_delay = 0.f,
                       float spawn_cooldown = 60.f,
                       std::int32_t health = -1) -> std::uint32_t;
auto add_turret_spawn(LevelSimInitData& data,
                      Vector3f location,
                      Rotator3f rotation,
                      Team team,
                      std::int32_t health = -1,
                      std::int32_t laser_damage = -1) -> std::uint32_t;
auto add_spinner_spawn(LevelSimInitData& data,
                       Vector3f location,
                       float yaw,
                       std::int32_t initial_fire_point_index) -> std::uint32_t;

}
