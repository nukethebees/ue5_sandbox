#include "SpaceGame/simulation/LevelSimulationBuilder.h"
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/simulation/SimulationConfigConversion.h>

#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/support/mesh.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml::level_simulation_builder {
void validate_fighter_spawn_slots(FCapitalSimulationConfig const& capital_config,
                                  FFighterSimulationConfig const& fighter_config,
                                  ioj::FEntityAABBs const& entity_bounds,
                                  float const fighter_radius,
                                  FLevelStartErrors& errors) {
    auto const capital_index{ioj::FEntityAABBs::capital_ship_index};
    auto const capital_centre{entity_bounds.get_centre(capital_index)};
    auto const fighter_clearance{fighter_radius + fighter_config.avoidance_clearance_buffer};
    FVector3f const clearance_extent{fighter_clearance, fighter_clearance, fighter_clearance};
    auto const capital_min{capital_centre - entity_bounds.get_half_extents(capital_index) -
                           clearance_extent};
    auto const capital_max{capital_centre + entity_bounds.get_half_extents(capital_index) +
                           clearance_extent};
    auto const& spawn_slots{capital_config.fighter_spawn_slots_relative_transforms};
    auto const slot_count{spawn_slots.Num()};

    for (int32 slot_index{}; slot_index < slot_count; ++slot_index) {
        auto const location{FVector3f{spawn_slots[slot_index].GetLocation()}};
        auto const inside_capital{location.X >= capital_min.X && location.X <= capital_max.X &&
                                  location.Y >= capital_min.Y && location.Y <= capital_max.Y &&
                                  location.Z >= capital_min.Z && location.Z <= capital_max.Z};
        if (inside_capital) {
            errors.add(FString::Printf(
                TEXT("fighter spawn slot %d at %s intersects the capital collision bounds plus "
                     "fighter clearance"),
                slot_index,
                *location.ToString()));
        }
    }

    auto const minimum_spacing{fighter_clearance * 2.f};
    auto const minimum_spacing_sq{minimum_spacing * minimum_spacing};
    for (int32 first_index{}; first_index < slot_count; ++first_index) {
        auto const first_location{FVector3f{spawn_slots[first_index].GetLocation()}};
        for (int32 second_index{first_index + 1}; second_index < slot_count; ++second_index) {
            auto const second_location{FVector3f{spawn_slots[second_index].GetLocation()}};
            if (FVector3f::DistSquared(first_location, second_location) < minimum_spacing_sq) {
                errors.add(FString::Printf(
                    TEXT("fighter spawn slots %d and %d are closer than the required %.1f cm "
                         "fighter clearance"),
                    first_index,
                    second_index,
                    minimum_spacing));
            }
        }
    }
}
}

namespace ml {
void validate_world_fighter_spawn_slots(FLevelSimulationInitData const& data,
                                        FLevelStartErrors& errors) {
    auto const clearance{data.fighter_radius + data.fighters.avoidance_clearance_buffer};
    auto const& slots{data.capital_ships.fighter_spawn_slots_relative_transforms};
    auto const validate{[&](FVectors3f::ConstView const locations,
                            FRotatorsf::ConstView const rotations) {
        auto const count{locations.num()};
        for (int32 i{}; i < count; ++i) {
            auto const position{get_vector3f(locations, i)};
            auto const rotation{FRotator3f{get_rotator3d(rotations, i)}};
            auto const bounds{
                ioj::make_entity_world_bounds(
                    data.entity_bounds, ioj::FEntityAABBs::capital_ship_index, position, rotation)
                    .ExpandBy(clearance)};
            auto const slot_count{slots.Num()};
            for (int32 slot_index{}; slot_index < slot_count; ++slot_index) {
                auto const spawn{position +
                                 rotation.RotateVector(FVector3f{slots[slot_index].GetLocation()})};
                if (bounds.IsInsideOrOn(spawn)) {
                    errors.add(FString::Printf(
                        TEXT("Capital at %s rotation %s: fighter spawn slot %d intersects world "
                             "collision bounds plus fighter clearance"),
                        *position.ToString(),
                        *rotation.ToString(),
                        slot_index));
                }
            }
        }
    }};
    validate(data.capital_spawns.locations.get_const_view(),
             data.capital_spawns.rotations.get_const_view());
    auto const& initial{data.level_events.initial_spawns.capital_spawns};
    validate(initial.locations.get_const_view(), initial.rotations.get_const_view());
    auto const& scheduled{data.level_events.schedule.capital_spawns};
    validate(scheduled.locations.get_const_view(), scheduled.rotations.get_const_view());
}

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
    level_simulation_builder::validate_fighter_spawn_slots(
        data.capital_ships, data.fighters, data.entity_bounds, data.fighter_radius, errors);
    if (errors.has_errors()) {
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(errors)};
    }
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
    FLevelStartErrors spawn_errors;
    validate_world_fighter_spawn_slots(data, spawn_errors);
    if (spawn_errors.has_errors()) {
        return FLevelSimulationBuildResult{std::unexpect, MoveTemp(spawn_errors)};
    }

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
