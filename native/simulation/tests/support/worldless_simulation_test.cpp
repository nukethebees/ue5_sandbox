#include "worldless_simulation_test.h"
#include <algorithm>
#include <cmath>
#include <ioj/sim/testing/level_sim_test_access.h>
namespace ioj::sim::tests {
WorldlessSimulationTest::WorldlessSimulationTest(LevelSimInitData data)
    : simulation_{std::move(data)} {}

void WorldlessSimulationTest::advance(time_type const dt) {
    auto const previous_tick{simulation_.get_clock().get_completed_ticks()};
    simulation_.advance(dt);
    if (simulation_.get_clock().get_completed_ticks() != previous_tick) {
        if (on_end_tick) {
            on_end_tick(simulation_);
        }
        timeline.tick(simulation_.get_clock().get_simulation_time());
    }
}

void WorldlessSimulationTest::queue_damage(std::span<EntityUniqueId const> const targets,
                                           std::int32_t const damage,
                                           EntityUniqueId const instigator) {
    DirectDamageEvents events;
    events.reserve(static_cast<std::int32_t>(targets.size()));
    for (auto const target : targets) {
        events.add(target, damage, instigator);
    }
    LevelSimTestAccess::queue_direct_damage_events(simulation_, events.get_const_view());
}
void WorldlessSimulationTest::queue_kills(std::span<EntityUniqueId const> const targets,
                                          EntityUniqueId const instigator) {
    DirectDamageEvents events;
    events.reserve(static_cast<std::int32_t>(targets.size()));
    for (auto const target : targets) {
        auto const state{simulation_.get_agent_accessor().read(target)};
        assert(state);
        events.add(target, std::max(1, state->health), instigator);
    }
    LevelSimTestAccess::queue_direct_damage_events(simulation_, events.get_const_view());
}

auto WorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    assert(maximum_time > 0.0);
    assert(simulation_.get_state() == OrchestratorState::Paused);
    simulation_.set_time_scale(1.0);
    simulation_.start();
    auto const tick_period{simulation_.get_clock().get_tick_period()};
    auto const maximum_ticks{static_cast<SimTick>(std::ceil(maximum_time / tick_period))};
    for (SimTick tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
        advance(tick_period);
    }
    simulation_.pause();
    return timeline.is_finished();
}
}

namespace ioj::sim::tests {
auto make_simulation_data(SimulationFixture const& fixture) -> LevelSimInitData {
    LevelSimInitData data;
    data.clock_settings = fixture.data.clock_settings;
    data.lasers = fixture.data.lasers;
    data.overlap_response = fixture.data.overlap_response;
    data.capital_ships = fixture.data.capital_ships;
    data.fighters = fixture.data.fighters;
    data.turrets = fixture.data.turrets;
    data.spinners = fixture.data.spinners;
    data.entity_bounds = fixture.data.entity_bounds;
    data.grid_dimensions = fixture.data.grid_dimensions;
    data.cell_size = fixture.data.cell_size;
    data.frame_memory_capacity_bytes = fixture.data.frame_memory_capacity_bytes;
    data.fighter_fire_point_distance = fixture.data.fighter_fire_point_distance;
    return data;
}
auto make_player_spawn(SimulationFixture const& fixture, Transform3d transform)
    -> player::PlayerSpawnData {
    auto player{fixture.player};
    player.transform = transform;
    return player;
}
auto add_player_spawn(LevelSimInitData& data, player::PlayerSpawnData spawn) -> std::int32_t {
    auto& initialisation{data.level_events.initialisation};
    auto const entity_index{initialisation.entity_count++};
    initialisation.player_entity_index = entity_index;
    data.player.emplace(spawn);
    return entity_index;
}
auto add_capital_spawn(LevelSimInitData& data,
                       Vector3f location,
                       Team team,
                       std::int32_t target_entity_index,
                       float initial_spawn_delay,
                       float spawn_cooldown,
                       std::int32_t health) -> std::int32_t {
    auto& storage{data.level_events.initial_spawns.capital_spawns};
    auto const row{storage.num()};
    auto const entity_index{data.level_events.initialisation.entity_count++};
    storage.add_defaulted(1);
    auto const events{storage.get_view().columns()};
    events.entity_indices[row] = entity_index;
    events.target_entity_indices[row] = target_entity_index;
    events.locations.set(row, location);
    events.teams[row] = team;
    events.healths[row] = health == -1 ? data.capital_ships.max_health : health;
    events.initial_fighter_spawn_delays[row] = initial_spawn_delay;
    events.fighter_spawn_cooldowns[row] = spawn_cooldown;
    return entity_index;
}
auto add_turret_spawn(LevelSimInitData& data,
                      Vector3f location,
                      Rotator3f rotation,
                      Team team,
                      std::int32_t health,
                      std::int32_t laser_damage) -> std::int32_t {
    auto& storage{data.level_events.initial_spawns.turret_spawns};
    auto const row{storage.num()};
    auto const entity_index{data.level_events.initialisation.entity_count++};
    storage.add_defaulted(1);
    auto const events{storage.get_view().columns()};
    events.entity_indices[row] = entity_index;
    events.locations.set(row, location);
    events.rotations.set(row, rotation);
    events.teams[row] = team;
    events.healths[row] = health == -1 ? data.turrets.max_health : health;
    events.laser_damages[row] = laser_damage == -1 ? data.turrets.laser.damage : laser_damage;
    return entity_index;
}
auto add_spinner_spawn(LevelSimInitData& data,
                       Vector3f location,
                       float const yaw,
                       std::int32_t const initial_fire_point_index) -> std::int32_t {
    auto& storage{data.level_events.initial_spawns.spinner_spawns};
    auto const row{storage.num()};
    auto const entity_index{data.level_events.initialisation.entity_count++};
    storage.add_defaulted(1);
    auto const events{storage.get_view().columns()};
    events.entity_indices[row] = entity_index;
    events.locations.set(row, location);
    events.yaws[row] = yaw;
    events.initial_fire_point_indices[row] = initial_fire_point_index;
    return entity_index;
}
}
