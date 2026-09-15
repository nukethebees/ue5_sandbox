#include "WorldlessSimulationTest.h"
#include <SpaceGame/simulation/SimulationConfigConversion.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/sim_config.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/support/mesh.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

namespace ml {
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
    result.config = make_simulation_config(config.player_ship);
    result.transform = ml::to_native(transform);
    return result;
}

auto add_worldless_player_spawn(::ioj::sim::LevelSimInitData& data,
                                ::ioj::sim::player::PlayerSpawnData spawn) -> int32 {
    auto& initialisation{data.level_events.initialisation};
    auto const entity_index{initialisation.entity_count++};
    initialisation.player_entity_index = entity_index;
    data.player.emplace(MoveTemp(spawn));
    return entity_index;
}

auto add_worldless_capital_spawn(::ioj::sim::LevelSimInitData& data,
                                 FVector3f const location,
                                 ETestTeam const team,
                                 int32 const target_entity_index,
                                 float const initial_spawn_delay,
                                 float const spawn_cooldown,
                                 int32 const health) -> int32 {
    auto& events{data.level_events.initial_spawns.capital_spawns};
    auto const index{events.num()};
    events.add_defaulted(1);
    events.entity_indices[index] = data.level_events.initialisation.entity_count++;
    events.target_entity_indices[index] = target_entity_index;
    events.locations.set(index, ml::to_native(FVector3f{location}));
    events.teams[index] = static_cast<::ioj::sim::Team>(team);
    events.healths[index] = health == INDEX_NONE ? data.capital_ships.max_health : health;
    events.initial_fighter_spawn_delays[index] = initial_spawn_delay;
    events.fighter_spawn_cooldowns[index] = spawn_cooldown;
    return index;
}

FWorldlessSimulationTest::FWorldlessSimulationTest(::ioj::sim::LevelSimInitData data)
    : simulation_{MoveTemp(data)} {
    simulation_.on_end_tick = [this](::ioj::sim::LevelSim& simulation) {
        if (on_end_tick) {
            on_end_tick(simulation);
        }
        timeline.tick(simulation.get_clock().get_simulation_time());
    };
}

void FWorldlessSimulationTest::finish_initialisation() {
    simulation_.finish_initialisation();
}

void FWorldlessSimulationTest::queue_damage(
    std::span<::ioj::sim::RegistryEntityHandle const> const targets,
    int32 const damage,
    ::ioj::sim::RegistryEntityHandle const instigator) {
    auto const count{static_cast<int32>(targets.size())};
    ::ioj::sim::DirectDamageEvents events;
    events.reserve(count);
    for (auto const target : targets) {
        events.add(target, damage, instigator);
    }
    get_registry().queue_direct_damage_events(events);
}

void FWorldlessSimulationTest::queue_kills(
    std::span<::ioj::sim::RegistryEntityHandle const> const targets,
    ::ioj::sim::RegistryEntityHandle const instigator) {
    ::ioj::sim::DirectDamageEvents events;
    auto const count{static_cast<int32>(targets.size())};
    events.reserve(count);
    for (auto const target : targets) {
        auto const damage{FMath::Max(1, get_registry().get_health(target))};
        events.add(target, damage, instigator);
    }
    get_registry().queue_direct_damage_events(events);
}

auto FWorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    check(maximum_time > 0.0);
    check(simulation_.get_state() == ::ioj::sim::OrchestratorState::Paused);
    simulation_.start();
    auto const tick_period{simulation_.get_clock().get_tick_period()};
    auto const maximum_ticks{static_cast<uint64>(FMath::CeilToDouble(maximum_time / tick_period))};
    for (uint64 tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
        simulation_.advance(tick_period);
    }
    simulation_.pause();
    return timeline.is_finished();
}
}
