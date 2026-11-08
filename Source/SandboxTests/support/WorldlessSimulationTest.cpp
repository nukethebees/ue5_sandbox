#include "WorldlessSimulationTest.h"

#include <ioj/column_math.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/support/mesh.h>

#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

namespace ioj {
auto make_worldless_simulation_test_data(USpaceGameLevelConfig const& config)
    -> ::ioj::sim::LevelSimInitData {
    check(IsValid(config.classes.player_ship_class));
    auto const* player{config.classes.player_ship_class->GetDefaultObject<ATestSpaceShip>()};
    check(player);
    auto result{make_level_simulation_init_data(config, {}, NullOpt, player->get_collision_mesh())};
    check(result);
    return MoveTemp(result.value());
}

auto make_worldless_player_spawn(USpaceGameLevelConfig const& config, FTransform const& transform)
    -> ::ioj::sim::player::PlayerSpawnData {
    check(IsValid(config.classes.player_ship_class));
    auto const* player{config.classes.player_ship_class->GetDefaultObject<ATestSpaceShip>()};
    check(player);
    auto result{player->make_spawn_data()};
    result.config = config.player_ship.simulation;
    result.health = {config.player_ship.simulation.health, config.player_ship.simulation.health};
    result.laser_mode = config.player_ship.simulation.laser_mode;
    result.laser_fire_rate = config.player_ship.simulation.laser_fire_rate;
    result.transform = to_native(transform);
    return result;
}

auto add_worldless_player_spawn(sim::LevelSimInitData& data, sim::player::PlayerSpawnData spawn)
    -> uint32 {
    auto const entity_index{data.level_events.spawn_count()};
    data.player_entity_ref = ioj::sim::LevelEntityRef{entity_index};
    data.player.emplace(MoveTemp(spawn));
    return entity_index;
}

FWorldlessSimulationTest::FWorldlessSimulationTest(sim::LevelSimInitData data)
    : simulation_{MoveTemp(data)} {}

void FWorldlessSimulationTest::advance(time_type const dt) {
    auto const previous_tick{simulation_.clock().get_completed_ticks().value()};
    simulation_.advance(dt);
    if (simulation_.clock().get_completed_ticks().value() != previous_tick) {
        if (on_end_tick) {
            on_end_tick(simulation_);
        }
        timeline.tick(simulation_.clock().simulation_time());
    }
}

void FWorldlessSimulationTest::queue_damage(std::span<sim::EntityId const> const targets,
                                            int32 const damage,
                                            sim::EntityId const instigator) {
    sim::DirectDamageEvents events;
    events.reserve(static_cast<int32>(targets.size()));
    for (auto const target : targets) {
        events.add(target, damage, instigator);
    }
    sim::LevelSimTestAccess::queue_direct_damage_events(simulation_, events.const_view());
}
void FWorldlessSimulationTest::queue_kills(std::span<sim::EntityId const> const targets,
                                           sim::EntityId const instigator) {
    sim::DirectDamageEvents events;
    events.reserve(static_cast<int32>(targets.size()));
    for (auto const target : targets) {
        auto const state{sim::observe_entity(simulation_, target)};
        check(state);
        events.add(target, FMath::Max(1, state->health.value()), instigator);
    }
    sim::LevelSimTestAccess::queue_direct_damage_events(simulation_, events.const_view());
}

auto FWorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    check(maximum_time > 0.0);
    check(simulation_.state() == sim::LevelState::Paused);
    simulation_.start();
    auto const tick_period{simulation_.clock().tick_period()};
    auto const maximum_ticks{static_cast<uint64>(FMath::CeilToDouble(maximum_time / tick_period))};
    for (uint64 tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
        advance(tick_period);
    }
    simulation_.pause();
    simulation_.advance(0.0);
    return timeline.is_finished();
}
}
