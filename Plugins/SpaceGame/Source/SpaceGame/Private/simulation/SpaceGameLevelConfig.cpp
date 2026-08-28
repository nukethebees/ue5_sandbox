#include "SpaceGame/simulation/SpaceGameLevelConfig.h"

#include <SpaceGame/effects/DelayedNiagaraSpawner.h>
#include <SpaceGame/ships/player/TestSpaceShipController.h>
#include <SpaceGame/simulation/SimulationActorClasses.h>

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

auto USpaceGameLevelConfig::is_valid() const noexcept -> bool {
    TArray<FString> errors;
    get_validation_errors(errors);
    return errors.IsEmpty();
}

void USpaceGameLevelConfig::get_validation_errors(TArray<FString>& errors) const {
#define REQUIRE_CONFIG(condition, message) \
    if (!(condition)) {                    \
        errors.Emplace(TEXT(message));     \
    }

    REQUIRE_CONFIG(classes.player_controller_class, "classes.player_controller_class is null");
    REQUIRE_CONFIG(classes.lasers_class, "classes.lasers_class is null");
    REQUIRE_CONFIG(classes.capital_ships_class, "classes.capital_ships_class is null");
    REQUIRE_CONFIG(classes.capital_ship_proxy_class, "classes.capital_ship_proxy_class is null");
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
    REQUIRE_CONFIG(laser_projectiles.max_cull_distance >= laser_projectiles.min_cull_distance,
                   "laser_projectiles cull distance range is invalid");
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
