#include "SpaceGame/simulation/LevelSimulationBuilder.h"

#include <SpaceGame/entities/TestEntityType.h>
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
                                     TOptional<test_space_ship::FPlayerSpawnData> player,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimulationBuildResult {
    FLevelStartErrors errors;
    auto const require_mesh{[&errors](UStaticMesh const* const mesh, TCHAR const* const name) {
        if (!IsValid(mesh)) {
            errors.add(FString::Printf(TEXT("%s is unavailable"), name));
        }
    }};
    require_mesh(config.capital_ships.mesh, TEXT("capital_ships.mesh"));
    require_mesh(config.fighters.mesh, TEXT("fighters.mesh"));
    require_mesh(config.turrets.mesh, TEXT("turrets.mesh"));
    require_mesh(config.tube_spinners.mesh, TEXT("tube_spinners.mesh"));

    if (player.IsSet()) {
        require_mesh(player_collision_mesh, TEXT("player ship mesh"));
    }
    if (errors.has_errors()) {
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(errors)};
    }

    FLevelSimulationBuildResult result{std::in_place};
    auto& data{result.value()};
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
    meshes[ETestEntityType::PlayerShip] = player_collision_mesh;
    meshes[ETestEntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[ETestEntityType::CapitalShipFighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    auto bounds{ioj::FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(bounds.error())};
    }
    data.entity_bounds = MoveTemp(bounds.value());
    return result;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     FLevelDefinition const& definition,
                                     TOptional<test_space_ship::FPlayerSpawnData> player,
                                     WorldAABBs static_bounds,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimulationBuildResult {
    auto const validation{validate_level(definition)};
    if (!validation) {
        FLevelStartErrors errors;
        for (auto const& error : validation.errors) {
            errors.add(error.message);
        }
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(errors)};
    }

    auto result{make_level_simulation_init_data(
        config, clock_settings, MoveTemp(player), player_collision_mesh)};
    if (!result) {
        return result;
    }
    auto& data{result.value()};
    data.static_bounds = MoveTemp(static_bounds);

    FSimulationClock clock;
    clock.initialise(clock_settings);
    auto compiled{compile_level_events(definition, clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(compiled.error())};
    }
    data.level_events = MoveTemp(compiled.value());

    auto const turret_events{data.level_events.initial_spawns.turret_spawns.get_const_view()};
    auto const turret_count{turret_events.num()};
    data.turret_transforms.Reserve(turret_count);
    for (int32 i{}; i < turret_count; ++i) {
        data.turret_transforms.Emplace(FRotator{get_rotator3d(turret_events.rotations, i)},
                                       FVector{get_vector3f(turret_events.locations, i)});
    }
    return result;
}
}
