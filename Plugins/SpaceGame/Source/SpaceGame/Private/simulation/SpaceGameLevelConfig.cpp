#include "SpaceGame/simulation/SpaceGameLevelConfig.h"

#include <ioj/sim/collision/collision_grid.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SimulationActorClasses.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <Engine/StaticMeshActor.h>

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

auto ioj::s7_level_config::load_canonical() -> USpaceGameLevelConfig* {
    return LoadObject<USpaceGameLevelConfig>(nullptr, canonical_object_path);
}

namespace {
auto collision_class_lists_are_valid(FCollisionGridConfig const& config) -> bool {
    for (auto const actor_class : config.harvested_collision_actor_classes) {
        if (!actor_class) {
            return false;
        }
    }
    for (auto const actor_class : config.omitted_collision_actor_classes) {
        if (!actor_class) {
            return false;
        }
    }

    for (auto const harvested_class : config.harvested_collision_actor_classes) {
        for (auto const omitted_class : config.omitted_collision_actor_classes) {
            if (harvested_class->IsChildOf(omitted_class) ||
                omitted_class->IsChildOf(harvested_class)) {
                return false;
            }
        }
    }

    return true;
}

auto collision_grid_dimensions_are_valid(FCollisionGridConfig const& config) -> bool {
    auto const dimensions{config.calculate_grid_dimensions()};
    return ::ioj::sim::collision::is_configured(
        {{dimensions.X, dimensions.Y, dimensions.Z}, ioj::Vector3f{config.cell_size}});
}
}

FScenarioClassConfig::FScenarioClassConfig()
    : static_turret_proxy_class{ATestStaticTurretsProxy::StaticClass()} {}

FCollisionGridConfig::FCollisionGridConfig()
    : harvested_collision_actor_classes{AStaticMeshActor::StaticClass()}
    , omitted_collision_actor_classes{ATestSpaceShip::StaticClass()} {}

auto FCollisionGridConfig::calculate_grid_dimensions() const noexcept -> FIntVector3 {
    auto const dimensions{::ioj::sim::collision::calculate_grid_dimensions(
        ioj::Vector3f{grid_size}, ioj::Vector3f{cell_size})};
    return {dimensions.x(), dimensions.y(), dimensions.z()};
}

auto FCollisionGridConfig::with_dimension_overrides(TOptional<FVector3f> grid_size_override,
                                                    TOptional<FVector3f> cell_size_override) const
    -> FCollisionGridConfig {
    auto result{*this};
    if (grid_size_override.IsSet()) {
        result.grid_size = grid_size_override.GetValue();
    }
    if (cell_size_override.IsSet()) {
        result.cell_size = cell_size_override.GetValue();
    }
    return result;
}

auto FCollisionGridConfig::is_valid() const noexcept -> bool {
    return collision_grid_dimensions_are_valid(*this) && collision_class_lists_are_valid(*this);
}

void USpaceGameLevelConfig::apply_presentation(
    ioj::settings::PresentationOverrides const& settings) {
    auto const apply{[](auto& target, auto const& patch) {
        if (patch) {
            apply_overrides(target, *patch);
        }
    }};
    apply(player_ship.presentation, settings.player_ship);
    apply(laser_projectiles.presentation, settings.laser_projectiles);
    apply(capital_ships.presentation, settings.capital_ships);
    apply(turrets.presentation, settings.turrets);
    apply(entity_overlay.values, settings.entity_overlay);
    apply(radar, settings.radar);
}

void USpaceGameLevelConfig::apply_settings(ioj::sim::SimulationSettings const& settings) {
    overlap_response = settings.overlap_response;
    player_ship.simulation = settings.player;
    player_ship.initial = settings.player_initial;
    laser_projectiles.simulation = settings.lasers;
    capital_ships.simulation = settings.capital_ships;
    capital_ships.simulation.fighter_spawn_slots =
        capital_ships.fighter_spawn_slots_relative_transforms.Num();
    fighters.simulation = settings.fighters;
    turrets.simulation = settings.turrets;
    tube_spinners.simulation = settings.spinners;
    collision_grid.grid_size = ioj::to_unreal(settings.collision_grid_size);
    collision_grid.cell_size = ioj::to_unreal(settings.collision_cell_size);
}

auto USpaceGameLevelConfig::is_valid(bool const require_presentation) const noexcept -> bool {
    TArray<FString> errors;
    get_validation_errors(errors, require_presentation);
    return errors.IsEmpty();
}

void USpaceGameLevelConfig::get_validation_errors(TArray<FString>& errors,
                                                  bool const require_presentation) const {
#define REQUIRE_CONFIG(condition, message) \
    if (!(condition)) {                    \
        errors.Emplace(TEXT(message));     \
    }

    if (require_presentation) {
        REQUIRE_CONFIG(classes.player_controller_class, "classes.player_controller_class is null");
        REQUIRE_CONFIG(laser_projectiles.mesh, "laser_projectiles.mesh is null");
        REQUIRE_CONFIG(laser_projectiles.material, "laser_projectiles.material is null");
        REQUIRE_CONFIG(capital_ships.team_visual_data, "capital_ships.team_visual_data is null");
        REQUIRE_CONFIG(fighters.team_visual_data, "fighters.team_visual_data is null");
        REQUIRE_CONFIG(turrets.team_visual_data, "turrets.team_visual_data is null");
        REQUIRE_CONFIG(!entity_overlay.values.enabled || entity_overlay.soft_target.mesh,
                       "entity_overlay.soft_target.mesh is null");
        REQUIRE_CONFIG(!entity_overlay.values.enabled || entity_overlay.soft_target.material,
                       "entity_overlay.soft_target.material is null");
    }
    REQUIRE_CONFIG(laser_projectiles.presentation.max_cull_distance >=
                       laser_projectiles.presentation.min_cull_distance,
                   "laser_projectiles cull distance range is invalid");
    REQUIRE_CONFIG(capital_ships.mesh, "capital_ships.mesh is null");
    REQUIRE_CONFIG(fighters.mesh, "fighters.mesh is null");
    REQUIRE_CONFIG(turrets.mesh, "turrets.mesh is null");
    REQUIRE_CONFIG(tube_spinners.mesh, "tube_spinners.mesh is null");
    REQUIRE_CONFIG(collision_grid.grid_size.X > 0.f && collision_grid.grid_size.Y > 0.f &&
                       collision_grid.grid_size.Z > 0.f,
                   "collision_grid.grid_size components must be positive");
    REQUIRE_CONFIG(collision_grid.cell_size.X > 0.f && collision_grid.cell_size.Y > 0.f &&
                       collision_grid.cell_size.Z > 0.f,
                   "collision_grid.cell_size components must be positive");
    REQUIRE_CONFIG(FMath::IsFinite(collision_grid.line_thickness) &&
                       collision_grid.line_thickness > 0.f,
                   "collision_grid.line_thickness must be finite and positive");
    REQUIRE_CONFIG(collision_grid_dimensions_are_valid(collision_grid),
                   "collision_grid calculated dimensions and cell count must fit in int32");
    REQUIRE_CONFIG(collision_class_lists_are_valid(collision_grid),
                   "collision_grid static collision actor class lists are invalid or overlap");

#undef REQUIRE_CONFIG
}

void USpaceGameLevelConfig::get_validation_warnings(TArray<FString>& warnings) const {
#define WARN_CONFIG(condition, message)  \
    if (!(condition)) {                  \
        warnings.Emplace(TEXT(message)); \
    }

    WARN_CONFIG(player_ship.team_visual_data, "player_ship.team_visual_data is null");

#undef WARN_CONFIG
}

#if WITH_EDITOR
EDataValidationResult USpaceGameLevelConfig::IsDataValid(FDataValidationContext& context) const {
    auto result{Super::IsDataValid(context)};

    TArray<FString> errors;
    get_validation_errors(errors);
    for (auto const& error : errors) {
        context.AddError(FText::FromString(error));
    }

    TArray<FString> warnings;
    get_validation_warnings(warnings);
    for (auto const& warning : warnings) {
        context.AddWarning(FText::FromString(warning));
    }

    if (!errors.IsEmpty()) {
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif
