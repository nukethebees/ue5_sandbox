#include "SpaceGame/simulation/LevelSimulationBuilder.h"
#include <sandbox/simulation/rotator_math.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/simulation/SimulationConfigConversion.h>

#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <vector>

namespace ml::level_simulation_builder {
auto intersects(simulation::collision::WorldAABB const& first,
                simulation::collision::WorldAABB const& second,
                float const first_clearance,
                float const second_clearance) noexcept -> bool {
    return first.min.X - first_clearance <= second.max.X + second_clearance &&
           first.max.X + first_clearance >= second.min.X - second_clearance &&
           first.min.Y - first_clearance <= second.max.Y + second_clearance &&
           first.max.Y + first_clearance >= second.min.Y - second_clearance &&
           first.min.Z - first_clearance <= second.max.Z + second_clearance &&
           first.max.Z + first_clearance >= second.min.Z - second_clearance;
}

void validate_fighter_spawn_slots(FCapitalSimulationConfig const& capital_config,
                                  FFighterSimulationConfig const& fighter_config,
                                  simulation::collision::EntityAABBs const& entity_bounds,
                                  FLevelStartErrors& errors) {
    auto const capital_index{ioj::FEntityAABBs::capital_ship_index};
    auto const fighter_index{ioj::FEntityAABBs::fighter_index};
    auto const capital_bounds{simulation::collision::make_entity_world_bounds(
        entity_bounds, capital_index, {}, ml::make_quaternion4f(0.0f, 0.0f, 0.0f, 1.0f))};
    auto const clearance{fighter_config.avoidance_clearance_buffer};
    auto const& spawn_slots{capital_config.fighter_spawn_slots_relative_transforms};
    auto const slot_count{static_cast<int32>(spawn_slots.size())};
    std::vector<simulation::collision::WorldAABB> fighter_bounds;
    fighter_bounds.reserve(static_cast<std::size_t>(slot_count));

    for (int32 slot_index{}; slot_index < slot_count; ++slot_index) {
        auto const& slot{spawn_slots[slot_index]};
        auto const location{ml::simulation::to_float(slot.location)};
        auto const orientation{ml::simulation::to_quaternion(
            ml::simulation::to_float(ml::simulation::to_rotator(slot.rotation)))};
        fighter_bounds.push_back(simulation::collision::make_entity_world_bounds(
            entity_bounds, fighter_index, location, orientation));
        if (intersects(capital_bounds, fighter_bounds.back(), 0.0f, clearance)) {
            errors.add(FString::Printf(
                TEXT("fighter spawn slot %d at %s intersects the capital collision bounds plus "
                     "fighter clearance"),
                slot_index,
                *ml::to_unreal(location).ToString()));
        }
    }

    for (int32 first_index{}; first_index < slot_count; ++first_index) {
        for (int32 second_index{first_index + 1}; second_index < slot_count; ++second_index) {
            if (intersects(fighter_bounds[first_index],
                           fighter_bounds[second_index],
                           clearance,
                           clearance)) {
                errors.add(FString::Printf(
                    TEXT("fighter spawn slots %d and %d have overlapping collision bounds plus "
                         "%.1f cm clearance"),
                    first_index,
                    second_index,
                    clearance));
            }
        }
    }
}
}

namespace ml {
void validate_world_fighter_spawn_slots(FLevelSimulationInitData const& data,
                                        FLevelStartErrors& errors) {
    auto const capital_index{ioj::FEntityAABBs::capital_ship_index};
    auto const fighter_index{ioj::FEntityAABBs::fighter_index};
    auto const clearance{data.fighters.avoidance_clearance_buffer};
    auto const& slots{data.capital_ships.fighter_spawn_slots_relative_transforms};
    auto const validate{[&](ml::simulation::Vectors3fConstView const locations,
                            ml::simulation::Rotators3fConstView const rotations) {
        auto const count{locations.num()};
        for (int32 i{}; i < count; ++i) {
            auto const position{ml::to_unreal(locations[i])};
            auto const rotation{ml::to_unreal(rotations[i])};
            auto const capital_orientation{simulation::to_quaternion(rotations[i])};
            auto const capital_bounds{simulation::collision::make_entity_world_bounds(
                data.entity_bounds, capital_index, locations[i], capital_orientation)};
            auto const slot_count{static_cast<int32>(slots.size())};
            for (int32 slot_index{}; slot_index < slot_count; ++slot_index) {
                auto const& slot{slots[slot_index]};
                auto const slot_location{ml::simulation::to_float(slot.location)};
                auto const spawn{position + rotation.RotateVector(ml::to_unreal(slot_location))};
                auto const slot_rotation{
                    ml::simulation::to_float(ml::simulation::to_rotator(slot.rotation))};
                auto const fighter_orientation{capital_orientation *
                                               simulation::to_quaternion(slot_rotation)};
                auto const fighter_bounds{simulation::collision::make_entity_world_bounds(
                    data.entity_bounds, fighter_index, ml::to_native(spawn), fighter_orientation)};
                if (level_simulation_builder::intersects(
                        capital_bounds, fighter_bounds, 0.0f, clearance)) {
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
    data.clock_settings = ml::to_native(clock_settings);
    data.lasers = make_simulation_config(config.laser_projectiles);
    data.capital_ships = make_simulation_config(config.capital_ships);
    data.capital_ships.fighter_spawn_slots_relative_transforms.resize(
        data.capital_ships.fighter_spawn_slots);
    data.fighters = make_simulation_config(config.fighters);
    data.fighters.max_live_fighters =
        GetDefault<USandboxDeveloperSettings>()->get_effective_max_live_fighters();
    data.turrets = make_simulation_config(config.turrets);
    data.spinners = make_simulation_config(config.tube_spinners);
    if (player.IsSet()) {
        data.player.emplace(MoveTemp(player.GetValue()));
    }
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighter_fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    auto const dimensions{config.collision_grid.calculate_grid_dimensions()};
    data.grid_dimensions = {dimensions.X, dimensions.Y, dimensions.Z};
    data.cell_size = ml::to_native(config.collision_grid.cell_size);

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
        data.capital_ships, data.fighters, data.entity_bounds, errors);
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
    data.participating_teams.reserve(definition.teams.Num());
    for (auto const team_id : definition.teams) {
        auto const team{resolve_level_team(team_id)};
        check(team.IsSet());
        data.participating_teams.add(ml::to_native(team.GetValue()));
    }

    FSimulationClock clock;
    clock.initialise(ml::to_native(clock_settings));
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
    data.turret_transforms.reserve(static_cast<std::size_t>(turret_count));
    for (int32 i{}; i < turret_count; ++i) {
        data.turret_transforms.push_back(
            ml::to_native(FTransform{FRotator{ml::to_unreal(turret_events.rotations[i])},
                                     FVector{ml::to_unreal(turret_events.locations[i])}}));
    }
    return result;
}
}
