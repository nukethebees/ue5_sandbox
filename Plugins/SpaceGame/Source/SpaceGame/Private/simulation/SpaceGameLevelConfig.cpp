#include "SpaceGame/simulation/SpaceGameLevelConfig.h"

#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/effects/DelayedNiagaraSpawner.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SimulationActorClasses.h>

#include <Engine/StaticMeshActor.h>

#include <limits>

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

namespace {
auto calculate_dimension(float const grid_size, float const cell_size) noexcept -> int32 {
    if (!FMath::IsFinite(grid_size) || !FMath::IsFinite(cell_size) || grid_size <= 0.f ||
        cell_size <= 0.f) {
        return 0;
    }

    auto const dimension{FMath::CeilToDouble(static_cast<double>(grid_size) / cell_size)};
    if (dimension > static_cast<double>(std::numeric_limits<int32>::max())) {
        return 0;
    }

    return static_cast<int32>(dimension);
}

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
    if (dimensions.X <= 0 || dimensions.Y <= 0 || dimensions.Z <= 0) {
        return false;
    }

    auto const xy_cell_count{static_cast<int64>(dimensions.X) * dimensions.Y};
    return xy_cell_count <= (std::numeric_limits<int32>::max() / dimensions.Z);
}
}

FCollisionGridConfig::FCollisionGridConfig()
    : harvested_collision_actor_classes{AStaticMeshActor::StaticClass()}
    , omitted_collision_actor_classes{ATestLasers::StaticClass(),
                                      ATestSpaceShip::StaticClass(),
                                      ATestCapitalShips::StaticClass(),
                                      ATestCapitalShipFighters::StaticClass(),
                                      ATestStaticTurrets::StaticClass(),
                                      ATestTubeSpinners::StaticClass()} {}

auto FCollisionGridConfig::calculate_grid_dimensions() const noexcept -> FIntVector3 {
    return {
        calculate_dimension(grid_size.X, cell_size.X),
        calculate_dimension(grid_size.Y, cell_size.Y),
        calculate_dimension(grid_size.Z, cell_size.Z),
    };
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
        REQUIRE_CONFIG(classes.lasers_class, "classes.lasers_class is null");
        REQUIRE_CONFIG(classes.capital_ships_class, "classes.capital_ships_class is null");
        REQUIRE_CONFIG(classes.capital_ship_fighters_class,
                       "classes.capital_ship_fighters_class is null");
        REQUIRE_CONFIG(classes.turrets_class, "classes.turrets_class is null");
        REQUIRE_CONFIG(classes.spinners_class, "classes.spinners_class is null");
        REQUIRE_CONFIG(classes.niagara_spawner_class, "classes.niagara_spawner_class is null");
        REQUIRE_CONFIG(player_ship.thrust_energy_max > 0.f,
                       "player_ship.thrust_energy_max must be positive");
        REQUIRE_CONFIG(player_ship.laser.projectile_speed > 0.f,
                       "player_ship.laser.projectile_speed must be positive");
        REQUIRE_CONFIG(player_ship.laser.max_distance > 0.f,
                       "player_ship.laser.max_distance must be positive");
    }
    REQUIRE_CONFIG(classes.capital_ship_proxy_class, "classes.capital_ship_proxy_class is null");
    REQUIRE_CONFIG(laser_projectiles.max_cull_distance >= laser_projectiles.min_cull_distance,
                   "laser_projectiles cull distance range is invalid");
    REQUIRE_CONFIG(laser_projectiles.n_preallocated_instances >= 0,
                   "laser_projectiles.n_preallocated_instances must not be negative");
    REQUIRE_CONFIG(laser_projectiles.collision_jobs > 0,
                   "laser_projectiles.collision_jobs must be positive");
    REQUIRE_CONFIG(turrets.search_slice_size > 0, "turrets.search_slice_size must be positive");
    REQUIRE_CONFIG(FMath::IsFinite(fighters.fire_dot_product_threshold) &&
                       fighters.fire_dot_product_threshold >= -1.f &&
                       fighters.fire_dot_product_threshold <= 1.f,
                   "fighters.fire_dot_product_threshold must be finite and between -1 and 1");
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
    WARN_CONFIG(laser_projectiles.mesh, "laser_projectiles.mesh is null");
    WARN_CONFIG(laser_projectiles.material, "laser_projectiles.material is null");
    WARN_CONFIG(capital_ships.mesh, "capital_ships.mesh is null");
    WARN_CONFIG(capital_ships.team_visual_data, "capital_ships.team_visual_data is null");
    WARN_CONFIG(fighters.mesh, "fighters.mesh is null");
    WARN_CONFIG(fighters.team_visual_data, "fighters.team_visual_data is null");
    WARN_CONFIG(turrets.mesh, "turrets.mesh is null");
    WARN_CONFIG(turrets.team_visual_data, "turrets.team_visual_data is null");
    WARN_CONFIG(tube_spinners.mesh, "tube_spinners.mesh is null");

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
