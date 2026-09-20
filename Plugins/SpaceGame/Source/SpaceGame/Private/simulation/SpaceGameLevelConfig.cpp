#include "SpaceGame/simulation/SpaceGameLevelConfig.h"

#include <ioj/sim/collision_grid.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SimulationActorClasses.h>
#include <SpaceGamePresentation/integration/VectorConversion.h>

#include <Engine/StaticMeshActor.h>

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

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
        {{dimensions.X, dimensions.Y, dimensions.Z}, ml::to_native(config.cell_size)});
}
}

FScenarioClassConfig::FScenarioClassConfig()
    : static_turret_proxy_class{ATestStaticTurretsProxy::StaticClass()} {}

FCollisionGridConfig::FCollisionGridConfig()
    : harvested_collision_actor_classes{AStaticMeshActor::StaticClass()}
    , omitted_collision_actor_classes{ATestSpaceShip::StaticClass()} {}

auto FCollisionGridConfig::calculate_grid_dimensions() const noexcept -> FIntVector3 {
    auto const dimensions{::ioj::sim::collision::calculate_grid_dimensions(
        ml::to_native(grid_size), ml::to_native(cell_size))};
    return {dimensions.x, dimensions.y, dimensions.z};
}

auto FCollisionGridConfig::is_valid() const noexcept -> bool {
    return collision_grid_dimensions_are_valid(*this) && collision_class_lists_are_valid(*this);
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
        REQUIRE_CONFIG(player_ship.thrust_energy_max > 0.f,
                       "player_ship.thrust_energy_max must be positive");
        REQUIRE_CONFIG(player_ship.laser.projectile_speed > 0.f,
                       "player_ship.laser.projectile_speed must be positive");
        REQUIRE_CONFIG(player_ship.laser.max_distance > 0.f,
                       "player_ship.laser.max_distance must be positive");
        REQUIRE_CONFIG(!entity_overlay.enabled || entity_overlay.soft_target.mesh,
                       "entity_overlay.soft_target.mesh is null");
        REQUIRE_CONFIG(!entity_overlay.enabled || entity_overlay.soft_target.material,
                       "entity_overlay.soft_target.material is null");
    }
    REQUIRE_CONFIG(FMath::IsFinite(player_ship.cruise_speed) && player_ship.cruise_speed > 0.f,
                   "player_ship.cruise_speed must be finite and positive");
    REQUIRE_CONFIG(FMath::IsFinite(player_ship.forward_velocity_trim_fraction) &&
                       player_ship.forward_velocity_trim_fraction > 0.f &&
                       player_ship.forward_velocity_trim_fraction <= 1.f,
                   "player_ship.forward_velocity_trim_fraction must be finite and in (0, 1]");
    REQUIRE_CONFIG(laser_projectiles.max_cull_distance >= laser_projectiles.min_cull_distance,
                   "laser_projectiles cull distance range is invalid");
    REQUIRE_CONFIG(laser_projectiles.n_preallocated_instances >= 0,
                   "laser_projectiles.n_preallocated_instances must not be negative");
    REQUIRE_CONFIG(laser_projectiles.collision_jobs > 0,
                   "laser_projectiles.collision_jobs must be positive");
    REQUIRE_CONFIG(capital_ships.mesh, "capital_ships.mesh is null");
    REQUIRE_CONFIG(fighters.mesh, "fighters.mesh is null");
    REQUIRE_CONFIG(turrets.mesh, "turrets.mesh is null");
    REQUIRE_CONFIG(tube_spinners.mesh, "tube_spinners.mesh is null");
    REQUIRE_CONFIG(turrets.search_slice_size > 0, "turrets.search_slice_size must be positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.fire_dot_product_threshold) &&
                       fighters.fire_dot_product_threshold >= -1.f &&
                       fighters.fire_dot_product_threshold <= 1.f,
                   "fighters.fire_dot_product_threshold must be finite and between -1 and 1");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_clear_update_frequency) &&
                       fighters.avoidance_clear_update_frequency > 0.f,
                   "fighters.avoidance_clear_update_frequency must be finite and positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_update_frequency) &&
                       fighters.avoidance_update_frequency > 0.f,
                   "fighters.avoidance_update_frequency must be finite and positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_active_update_frequency) &&
                       fighters.avoidance_active_update_frequency > 0.f,
                   "fighters.avoidance_active_update_frequency must be finite and positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_immediate_update_frequency) &&
                       fighters.avoidance_immediate_update_frequency > 0.f,
                   "fighters.avoidance_immediate_update_frequency must be finite and positive");
    REQUIRE_CONFIG(
        fighters.avoidance_clear_update_frequency <= fighters.avoidance_update_frequency &&
            fighters.avoidance_update_frequency <= fighters.avoidance_active_update_frequency &&
            fighters.avoidance_active_update_frequency <=
                fighters.avoidance_immediate_update_frequency,
        "fighter avoidance update frequencies must be non-decreasing by risk tier");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_lookahead_time) &&
                       fighters.avoidance_lookahead_time >= 0.f,
                   "fighters.avoidance_lookahead_time must be finite and non-negative");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.avoidance_clearance_buffer) &&
                       fighters.avoidance_clearance_buffer >= 0.f,
                   "fighters.avoidance_clearance_buffer must be finite and non-negative");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.separation_radius) && fighters.separation_radius > 0.f,
                   "fighters.separation_radius must be finite and positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.separation_strength) &&
                       fighters.separation_strength >= 0.f,
                   "fighters.separation_strength must be finite and non-negative");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.steering_memory_duration) &&
                       fighters.steering_memory_duration >= 0.f,
                   "fighters.steering_memory_duration must be finite and non-negative");
    REQUIRE_CONFIG(fighters.dense_traffic_neighbour_threshold >= 2,
                   "fighters.dense_traffic_neighbour_threshold must be at least two");
    REQUIRE_CONFIG(capital_ships.fighter_spawn_slots >= 0,
                   "capital_ships.fighter_spawn_slots must not be negative");
    REQUIRE_CONFIG(capital_ships.fighter_spawn_slots ==
                       capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                   "capital_ships fighter spawn slot count does not match its transforms");
    REQUIRE_CONFIG(capital_ships.max_health > 0, "capital_ships.max_health must be positive");
    REQUIRE_CONFIG(fighters.health > 0, "fighters.health must be positive");
    REQUIRE_CONFIG(fighters.laser.projectile_speed > 0.f,
                   "fighters.laser.projectile_speed must be positive");
    REQUIRE_CONFIG(turrets.max_health > 0, "turrets.max_health must be positive");
    REQUIRE_CONFIG(turrets.laser.projectile_speed > 0.f,
                   "turrets.laser.projectile_speed must be positive");
    REQUIRE_CONFIG(tube_spinners.laser.projectile_speed > 0.f,
                   "tube_spinners.laser.projectile_speed must be positive");
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
