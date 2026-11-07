#include "worldless_simulation_test.h"

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <cmath>
namespace ioj::sim::tests {
WorldlessSimulationTest::WorldlessSimulationTest(LevelSimInitData data)
    : simulation_{std::move(data)} {}

void WorldlessSimulationTest::advance(time_type const dt) {
    auto const previous_tick{simulation_.clock().get_completed_ticks()};
    simulation_.advance(dt, &navigation_counters);
    if (simulation_.clock().get_completed_ticks() != previous_tick) {
        if (on_end_tick) {
            on_end_tick(simulation_);
        }
        timeline.tick(simulation_.clock().simulation_time());
    }
}

void WorldlessSimulationTest::queue_damage(std::span<EntityId const> const targets,
                                           std::int32_t const damage,
                                           EntityId const instigator) {
    DirectDamageEvents events;
    events.reserve(static_cast<std::int32_t>(targets.size()));
    for (auto const target : targets) {
        events.add(target, damage, instigator);
    }
    LevelSimTestAccess::queue_direct_damage_events(simulation_, events.const_view());
}
void WorldlessSimulationTest::queue_kills(std::span<EntityId const> const targets,
                                          EntityId const instigator) {
    DirectDamageEvents events;
    events.reserve(static_cast<std::int32_t>(targets.size()));
    for (auto const target : targets) {
        auto const state{observe_entity(simulation_, target)};
        assert(state);
        events.add(target, std::max(1, state->health.value()), instigator);
    }
    LevelSimTestAccess::queue_direct_damage_events(simulation_, events.const_view());
}

auto WorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    assert(maximum_time > 0.0);
    assert(simulation_.state() == LevelState::Paused);
    simulation_.start();
    auto const tick_period{simulation_.clock().tick_period()};
    auto const maximum_ticks{
        static_cast<SimTick::value_type>(std::ceil(maximum_time / tick_period))};
    // NOLINTNEXTLINE(ioj-loop-condition-call) -- advancing the simulation can finish the timeline.
    for (SimTick::value_type tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
        advance(tick_period);
    }
    simulation_.pause();
    simulation_.advance(0.0);
    return timeline.is_finished();
}
}

namespace ioj::sim::tests {
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData {
    LevelSimInitData data;
    data.clock_settings = fixture.data.clock_settings;
    data.lasers = fixture.data.lasers;
    data.collision.overlap_response = fixture.data.collision.overlap_response;
    data.capital_ships = fixture.data.capital_ships;
    data.fighters = fixture.data.fighters;
    data.turrets = fixture.data.turrets;
    data.spinners = fixture.data.spinners;
    data.collision.entity_bounds = fixture.data.collision.entity_bounds;
    data.collision.grid_geometry = fixture.data.collision.grid_geometry;
    data.tick_memory_capacity_bytes = fixture.data.tick_memory_capacity_bytes;
    data.phase_memory_capacity_bytes = fixture.data.phase_memory_capacity_bytes;
    return data;
}
auto make_player_spawn(SimulationFixture const& fixture, Transform3d transform)
    -> player::PlayerSpawnData {
    auto player{fixture.player};
    player.transform = transform;
    return player;
}
auto SimulationScenario::compile() && -> LevelSimInitData {
    if (entities.empty()) {
        assert(!mission);
        return std::move(runtime);
    }

    levels::LevelDefinitionComponents authored{
        .metadata = {.id = levels::LevelId{"test-scenario"}, .title = "Test scenario"},
        .mission = std::move(mission),
        .entities = std::move(entities),
    };
    auto const player{std::ranges::find(authored.entities,
                                        levels::EntityArchetype::PlayerFighter,
                                        &levels::EntitySpawnDefinition::archetype)};
    if (player != authored.entities.end()) {
        authored.player_entity_id = player->id;
    } else {
        authored.camera = levels::LevelCameraDefinition{
            .target_entity_ids = {authored.entities.front().id},
            .offset_direction = {-1., 0., 0.},
            .distance = 1000.,
        };
    }
    auto definition{levels::LevelDefinition::make(std::move(authored))};
    assert(definition);
    auto compiled{levels::compile_level(*definition, runtime)};
    assert(compiled);
    return std::move(runtime);
}

auto add_player_spawn(SimulationScenario& data, player::PlayerSpawnData spawn) -> std::uint32_t {
    auto const index{static_cast<std::uint32_t>(data.entities.size())};
    data.entities.push_back({
        .id = levels::EntityId{"entity-" + std::to_string(index)},
        .archetype = levels::EntityArchetype::PlayerFighter,
        .team = spawn.team,
        .position = spawn.transform.location,
        .rotation = spawn.transform.rotator(),
    });
    data.runtime.player.emplace(std::move(spawn));
    return index;
}
auto add_capital_spawn(SimulationScenario& data,
                       Vector3f location,
                       Team team,
                       float initial_spawn_delay,
                       float spawn_cooldown,
                       std::optional<settings::PositiveHealth> health) -> std::uint32_t {
    auto const index{static_cast<std::uint32_t>(data.entities.size())};
    data.entities.push_back({
        .id = levels::EntityId{"entity-" + std::to_string(index)},
        .archetype = levels::EntityArchetype::CapitalShip,
        .team = team,
        .position = Vector3d{location},
        .overrides =
            {
                .health = health,
                .initial_spawn_delay = NonNegativeFinite<float>::make(initial_spawn_delay),
                .spawn_cooldown = NonNegativeFinite<float>::make(spawn_cooldown),
            },
    });
    return index;
}
auto add_turret_spawn(SimulationScenario& data,
                      Vector3f location,
                      Rotator3f rotation,
                      Team team,
                      std::optional<settings::PositiveHealth> health,
                      std::optional<NonNegative<std::int32_t>> laser_damage) -> std::uint32_t {
    auto const index{static_cast<std::uint32_t>(data.entities.size())};
    data.entities.push_back({
        .id = levels::EntityId{"entity-" + std::to_string(index)},
        .archetype = levels::EntityArchetype::StaticTurret,
        .team = team,
        .position = Vector3d{location},
        .rotation = {rotation.pitch, rotation.yaw, rotation.roll},
        .overrides = {.health = health, .laser_damage = laser_damage},
    });
    return index;
}
auto add_spinner_spawn(SimulationScenario& data,
                       Vector3f location,
                       float const yaw,
                       std::int32_t const initial_fire_point_index) -> std::uint32_t {
    auto const index{static_cast<std::uint32_t>(data.entities.size())};
    data.entities.push_back({
        .id = levels::EntityId{"entity-" + std::to_string(index)},
        .archetype = levels::EntityArchetype::TubeSpinner,
        .team = Team::White,
        .position = Vector3d{location},
        .rotation = {0., yaw, 0.},
        .overrides = {.initial_fire_point =
                          NonNegative<std::int32_t>::make(initial_fire_point_index)},
    });
    return index;
}
}
