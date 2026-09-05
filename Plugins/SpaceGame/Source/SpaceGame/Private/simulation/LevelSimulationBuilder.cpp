#include "SpaceGame/simulation/LevelSimulationBuilder.h"

#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/mesh.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml {
auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     TOptional<test_space_ship::FPlayerSpawnData> player)
    -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.clock_settings = clock_settings;
    data.lasers = make_simulation_config(config.laser_projectiles);
    data.capital_ships = make_simulation_config(config.capital_ships);
    data.capital_ships.fighter_spawn_slots_relative_transforms.SetNum(
        data.capital_ships.fighter_spawn_slots);
    data.fighters = make_simulation_config(config.fighters);
    data.turrets = make_simulation_config(config.turrets);
    data.spinners = make_simulation_config(config.tube_spinners);
    data.player = MoveTemp(player);
    data.capital_radius = get_mesh_sphere_bounds(*config.capital_ships.mesh);
    data.fighter_radius = get_mesh_sphere_bounds(*config.fighters.mesh);
    data.turret_radius = get_mesh_sphere_bounds(*config.turrets.mesh);
    data.spinner_radius = get_mesh_sphere_bounds(*config.tube_spinners.mesh);
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighter_fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    data.grid_dimensions = config.collision_grid.calculate_grid_dimensions();
    data.cell_size = config.collision_grid.cell_size;

    ioj::FLevelCollisionHost::EntityMeshes meshes{};
    if (IsValid(config.classes.player_ship_class)) {
        auto const* player_cdo{
            config.classes.player_ship_class->GetDefaultObject<ATestSpaceShip>()};
        meshes[ETestEntityType::PlayerShip] =
            player_cdo ? player_cdo->get_collision_mesh() : nullptr;
    }
    meshes[ETestEntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[ETestEntityType::CapitalShipFighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    data.entity_bounds = ioj::FLevelCollisionHost::extract_entity_bounds(meshes);
    return data;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     FLevelDefinition const& definition,
                                     TOptional<test_space_ship::FPlayerSpawnData> player,
                                     WorldAABBs static_bounds) -> FLevelSimulationInitData {
    check(validate_level(definition));

    auto data{make_level_simulation_init_data(config, clock_settings, MoveTemp(player))};
    data.static_bounds = MoveTemp(static_bounds);

    FSimulationClock clock;
    clock.initialise(clock_settings);
    data.level_events = compile_level_events(definition, clock, data.capital_ships, data.turrets);

    auto const& schedule{data.level_events.schedule};
    auto const group_count{!schedule.execution_ticks.IsEmpty() && schedule.execution_ticks[0] == 0
                               ? schedule.event_group_counts[0].spawn_groups
                               : 0};
    auto const spawn_groups{schedule.spawn_groups.get_const_view(0, group_count)};
    for (int32 group_index{}; group_index < group_count; ++group_index) {
        if (spawn_groups.types[group_index] == ETestEntityType::Turret) {
            auto const turret_count{spawn_groups.counts[group_index]};
            auto const turret_events{schedule.turret_spawns.get_const_view(
                spawn_groups.offsets[group_index], turret_count)};
            for (int32 i{}; i < turret_count; ++i) {
                data.turret_transforms.Emplace(FRotator{get_rotator3d(turret_events.rotations, i)},
                                               FVector{get_vector3f(turret_events.locations, i)});
            }
        }
    }
    return data;
}
}
