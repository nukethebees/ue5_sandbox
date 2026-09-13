#include "worldless_simulation_test.h"
#include <algorithm>
#include <cmath>
namespace ml::simulation_tests {
WorldlessSimulationTest::WorldlessSimulationTest(FLevelSimulationInitData data)
    : simulation_{std::move(data)} {
    simulation_.on_end_tick = [this](FLevelSimulation& simulation) {
        if (on_end_tick) {
            on_end_tick(simulation);
        }
        timeline.tick(simulation.get_clock().get_simulation_time());
    };
}

void WorldlessSimulationTest::queue_damage(std::span<FRegistryEntityHandle const> const targets,
                                           std::int32_t const damage,
                                           FRegistryEntityHandle const instigator) {
    auto const count{static_cast<std::int32_t>(targets.size())};
    DirectDamageEvents events;
    events.reserve(count);
    for (auto const target : targets) {
        events.add(target, damage, instigator);
    }
    get_registry().queue_direct_damage_events(events);
}

void WorldlessSimulationTest::queue_kills(std::span<FRegistryEntityHandle const> const targets,
                                          FRegistryEntityHandle const instigator) {
    DirectDamageEvents events;
    auto const count{static_cast<std::int32_t>(targets.size())};
    events.reserve(count);
    for (auto const target : targets) {
        auto const damage{std::max(1, get_registry().get_health(target))};
        events.add(target, damage, instigator);
    }
    get_registry().queue_direct_damage_events(events);
}

auto WorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    assert(maximum_time > 0.0);
    assert(simulation_.get_state() == EOrchestratorState::Paused);
    simulation_.start();
    auto const tick_period{simulation_.get_clock().get_tick_period()};
    auto const maximum_ticks{static_cast<std::uint64_t>(std::ceil(maximum_time / tick_period))};
    for (std::uint64_t tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
        simulation_.advance(tick_period);
    }
    simulation_.pause();
    return timeline.is_finished();
}
}

namespace ml::simulation_tests {
auto make_simulation_data(SimulationFixture const& fixture) -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
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
auto make_player_spawn(SimulationFixture const& fixture, ml::simulation::Transform3d transform)
    -> ml::test_space_ship::FPlayerSpawnData {
    auto player{fixture.player};
    player.transform = transform;
    return player;
}
auto add_capital_spawn(FLevelSimulationInitData& data,
                       ml::simulation::Vector3f location,
                       ml::simulation::Team team,
                       std::int32_t target_spawn_index,
                       float initial_spawn_delay,
                       float spawn_cooldown,
                       std::int32_t health) -> std::int32_t {
    auto const index{data.capital_spawns.num()};
    data.capital_spawns.add_defaulted(1);
    data.capital_spawns.locations.set(index, location);
    data.capital_spawns.teams[index] = team;
    data.capital_spawns.healths[index] = health == -1 ? data.capital_ships.max_health : health;
    data.capital_spawns.initial_spawn_delays[index] = initial_spawn_delay;
    data.capital_spawns.spawn_cooldowns[index] = spawn_cooldown;
    data.capital_target_spawn_indices.push_back(target_spawn_index);
    return index;
}
}
