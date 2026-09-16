#include "SpaceGame/simulation/LevelSimulationBuilder.h"
#include <ioj/sim/levels/fighter_spawn_slot_validation.h>
#include <ioj/sim/rotator_math.h>
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
void append_fighter_spawn_slot_errors(
    std::vector<::ioj::sim::levels::FighterSpawnSlotValidationError> const& validation_errors,
    ::ioj::sim::CapitalShipSimConfig const& capital_config,
    FLevelStartErrors& errors) {
    for (auto const& error : validation_errors) {
        switch (error.kind) {
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::IntersectsCapital: {
                auto const location{::ioj::sim::to_float(
                    capital_config.fighter_spawn_slots_relative_transforms[error.first_slot]
                        .location)};
                errors.add(FString::Printf(
                    TEXT("fighter spawn slot %d at %s intersects the capital collision bounds plus "
                         "fighter clearance"),
                    error.first_slot,
                    *ml::to_unreal(location).ToString()));
                break;
            }
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::SlotsOverlap:
                errors.add(FString::Printf(
                    TEXT("fighter spawn slots %d and %d have overlapping collision bounds plus "
                         "%.1f cm clearance"),
                    error.first_slot,
                    error.second_slot,
                    error.clearance));
                break;
            case ::ioj::sim::levels::FighterSpawnSlotValidationErrorKind::WorldIntersectsCapital:
                errors.add(FString::Printf(
                    TEXT("Capital at %s rotation %s: fighter spawn slot %d intersects world "
                         "collision bounds plus fighter clearance"),
                    *ml::to_unreal(error.capital_position).ToString(),
                    *ml::to_unreal(error.capital_rotation).ToString(),
                    error.first_slot));
                break;
        }
    }
}

void validate_fighter_spawn_slots(::ioj::sim::CapitalShipSimConfig const& capital_config,
                                  ::ioj::sim::FighterSimConfig const& fighter_config,
                                  ::ioj::sim::collision::EntityAABBs const& entity_bounds,
                                  FLevelStartErrors& errors) {
    append_fighter_spawn_slot_errors(::ioj::sim::levels::validate_fighter_spawn_slots(
                                         capital_config, fighter_config, entity_bounds),
                                     capital_config,
                                     errors);
}
}

namespace ml {
void validate_world_fighter_spawn_slots(::ioj::sim::LevelSimInitData const& data,
                                        FLevelStartErrors& errors) {
    level_simulation_builder::append_fighter_spawn_slot_errors(
        ::ioj::sim::levels::validate_world_fighter_spawn_slots(data), data.capital_ships, errors);
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     TOptional<::ioj::sim::player::PlayerSpawnData> player,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
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
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }

    FLevelSimBuildResult result{std::in_place};
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
    meshes[ETestEntityType::Fighter] = config.fighters.mesh;
    meshes[ETestEntityType::Turret] = config.turrets.mesh;
    meshes[ETestEntityType::TubeSpinner] = config.tube_spinners.mesh;
    auto bounds{ioj::FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(bounds.error())};
    }
    data.entity_bounds = MoveTemp(bounds.value());
    level_simulation_builder::validate_fighter_spawn_slots(
        data.capital_ships, data.fighters, data.entity_bounds, errors);
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     FFixedTickLoop const& clock_settings,
                                     FLevelDefinition const& definition,
                                     TOptional<::ioj::sim::player::PlayerSpawnData> player,
                                     WorldAABBs static_bounds,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    auto const validation{validate_level(definition)};
    if (!validation) {
        FLevelStartErrors errors;
        for (auto const& error : validation.errors) {
            errors.add(error.message);
        }
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
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

    ::ioj::sim::SimClock clock;
    clock.initialise(ml::to_native(clock_settings));
    auto compiled{compile_level_events(definition, clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(compiled.error())};
    }
    data.level_events = MoveTemp(compiled.value());
    FLevelStartErrors spawn_errors;
    validate_world_fighter_spawn_slots(data, spawn_errors);
    if (spawn_errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(spawn_errors)};
    }

    return result;
}
}
