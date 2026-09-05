#include "WorldlessSimulationTest.h"

#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/mesh.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

namespace ml {
auto make_worldless_simulation_test_data(USpaceGameLevelConfig const& config)
    -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.lasers = make_simulation_config(config.laser_projectiles);
    data.capital_ships = make_simulation_config(config.capital_ships);
    data.capital_ships.fighter_spawn_slots_relative_transforms.SetNum(
        data.capital_ships.fighter_spawn_slots);
    data.fighters = make_simulation_config(config.fighters);
    data.turrets = make_simulation_config(config.turrets);
    data.spinners = make_simulation_config(config.tube_spinners);
    data.capital_radius = ml::get_mesh_sphere_bounds(*config.capital_ships.mesh);
    data.fighter_radius = ml::get_mesh_sphere_bounds(*config.fighters.mesh);
    data.turret_radius = ml::get_mesh_sphere_bounds(*config.turrets.mesh);
    data.spinner_radius = ml::get_mesh_sphere_bounds(*config.tube_spinners.mesh);
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighter_fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    data.grid_dimensions = config.collision_grid.calculate_grid_dimensions();
    data.cell_size = config.collision_grid.cell_size;

    ml::ioj::FLevelCollisionHost::EntityMeshes meshes{};
    if (IsValid(config.classes.player_ship_class)) {
        auto const* player{config.classes.player_ship_class->GetDefaultObject<ATestSpaceShip>()};
        meshes[ETestEntityType::PlayerShip] = player ? player->get_collision_mesh() : nullptr;
    }
    meshes[ETestEntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[ETestEntityType::CapitalShipFighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    data.entity_bounds = ml::ioj::FLevelCollisionHost::extract_entity_bounds(meshes);
    return data;
}

auto make_worldless_player_spawn(USpaceGameLevelConfig const& config, FTransform const& transform)
    -> ml::test_space_ship::FPlayerSpawnData {
    check(IsValid(config.classes.player_ship_class));
    auto const* player{config.classes.player_ship_class->GetDefaultObject<ATestSpaceShip>()};
    check(player);
    auto result{player->make_spawn_data()};
    result.config = make_simulation_config(config.player_ship);
    result.transform = transform;
    return result;
}

auto add_worldless_capital_spawn(FLevelSimulationInitData& data,
                                 FVector3f const location,
                                 ETestTeam const team,
                                 int32 const target_spawn_index,
                                 float const initial_spawn_delay,
                                 float const spawn_cooldown,
                                 int32 const health) -> int32 {
    auto const index{data.capital_spawns.num()};
    data.capital_spawns.add_defaulted(1);
    data.capital_spawns.locations.set(index, location);
    data.capital_spawns.teams[index] = team;
    data.capital_spawns.healths[index] =
        health == INDEX_NONE ? data.capital_ships.max_health : health;
    data.capital_spawns.initial_spawn_delays[index] = initial_spawn_delay;
    data.capital_spawns.spawn_cooldowns[index] = spawn_cooldown;
    data.capital_target_spawn_indices.Add(target_spawn_index);
    return index;
}

FWorldlessSimulationTest::FWorldlessSimulationTest(FLevelSimulationInitData data)
    : simulation_{MoveTemp(data)} {
    simulation_.on_end_tick = [this](FLevelSimulation& simulation) {
        if (on_end_tick) {
            on_end_tick(simulation);
        }
        timeline.tick(simulation.get_clock().get_simulation_time());
    };
}

void FWorldlessSimulationTest::finish_initialisation() {
    simulation_.finish_initialisation();
}

void FWorldlessSimulationTest::queue_damage(TConstArrayView<FRegistryEntityHandle> const targets,
                                            int32 const damage,
                                            FRegistryEntityHandle const instigator) {
    auto const count{targets.Num()};
    DirectDamageEvents events;
    events.add_uninitialised(count);
    events.damaged_entities = targets;
    ml::fill(events.damage_amounts, damage);
    ml::fill(events.instigators, instigator);
    get_registry().queue_direct_damage_events(events);
}

void FWorldlessSimulationTest::queue_kills(TConstArrayView<FRegistryEntityHandle> const targets,
                                           FRegistryEntityHandle const instigator) {
    DirectDamageEvents events;
    auto const count{targets.Num()};
    events.add_uninitialised(count);
    events.damaged_entities = targets;
    ml::fill(events.instigators, instigator);
    for (int32 i{}; i < count; ++i) {
        events.damage_amounts[i] = FMath::Max(1, get_registry().get_health(targets[i]));
    }
    get_registry().queue_direct_damage_events(events);
}

auto FWorldlessSimulationTest::run_until_timeline_finished(time_type const maximum_time) -> bool {
    check(maximum_time > 0.0);
    check(simulation_.get_state() == EOrchestratorState::Paused);
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
