#pragma once
#include <ioj/levels/level_definition_components.h>
#include <ioj/sim/levels/reference_level_simulation_data.h>

namespace ioj::sim::tests {
using SimulationFixture = ReferenceLevelSimulationData;

struct SimulationScenario {
    explicit SimulationScenario(LevelSimInitData data = {})
        : runtime{std::move(data)} {}
    auto compile() && -> LevelSimInitData;

    LevelSimInitData runtime;
    std::vector<levels::EntitySpawnDefinition> entities;
    levels::LogicDefinition logic;
};
auto make_fixture() -> SimulationFixture;
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData;
auto make_player_spawn(SimulationFixture const& fixture, Transform3d transform = {})
    -> player::PlayerSpawnData;
auto add_player_spawn(SimulationScenario& data, player::PlayerSpawnData spawn) -> std::uint32_t;
auto add_capital_spawn(SimulationScenario& data,
                       Vector3f location,
                       Team team,
                       float initial_spawn_delay = 0.f,
                       float spawn_cooldown = 60.f,
                       std::optional<settings::PositiveHealth> health = {}) -> std::uint32_t;
auto add_turret_spawn(SimulationScenario& data,
                      Vector3f location,
                      Rotator3f rotation,
                      Team team,
                      std::optional<settings::PositiveHealth> health = {},
                      std::optional<NonNegative<std::int32_t>> laser_damage = {}) -> std::uint32_t;
auto add_spinner_spawn(SimulationScenario& data,
                       Vector3f location,
                       float yaw,
                       std::int32_t initial_fire_point_index) -> std::uint32_t;

}
