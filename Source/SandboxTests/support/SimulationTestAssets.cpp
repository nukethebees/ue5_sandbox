#include "SimulationTestAssets.h"

#include <SandboxTests/SandboxTestLogCategories.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <UObject/Package.h>
#include <UObject/SoftObjectPath.h>
#include <UObject/SoftObjectPtr.h>
#include <UObject/StrongObjectPtr.h>
#include <UObject/UObjectGlobals.h>

namespace ioj {
auto load_default_level_config() -> USpaceGameLevelConfig* {
    static TSoftObjectPtr<USpaceGameLevelConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_FT_soa_entities_LevelConfig."
                             "DA_FT_soa_entities_LevelConfig")}};

    static TStrongObjectPtr<USpaceGameLevelConfig> resolved;
    if (!resolved.IsValid()) {
        auto* const resources{default_config.LoadSynchronous()};
        if (!IsValid(resources)) {
            UE_LOG(
                LogSandboxTest, Error, TEXT("Cannot load default simulation-test asset package"));
            return nullptr;
        }
        auto const loaded{read_game_settings(TEXT("LevelScripts/Settings/base.scm"),
                                             nullptr,
                                             {},
                                             *resources,
                                             GetTransientPackage())};
        check(loaded);
        resolved.Reset(loaded->config);
    }
    auto* const config{resolved.Get()};
    if (IsValid(config)) {
        UE_LOG(LogSandboxTest,
               Log,
               TEXT("Loaded default simulation-test asset package: %s"),
               *config->GetPackage()->GetName());
    }
    return config;
}

auto get_default_level_config(FSoftTestAssertions& checks) -> USpaceGameLevelConfig const* {
    auto* const config{load_default_level_config()};
    if (!checks.not_nullptr(config, TEXT("Default level config loads"))) {
        return nullptr;
    }
    if (!checks.is_true(config->is_valid(), TEXT("Default level config is valid"))) {
        return nullptr;
    }
    return config;
}

auto duplicate_level_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> USpaceGameLevelConfig* {
    auto* const copy{DuplicateObject<USpaceGameLevelConfig>(&config, &outer)};
    check(copy);
    copy->player_ship = config.player_ship;
    copy->laser_projectiles = config.laser_projectiles;
    copy->capital_ships = config.capital_ships;
    copy->fighters = config.fighters;
    copy->turrets = config.turrets;
    copy->tube_spinners = config.tube_spinners;
    copy->collision_grid = config.collision_grid;
    copy->entity_overlay = config.entity_overlay;
    copy->radar = config.radar;
    copy->overlap_response = config.overlap_response;
    return copy;
}

auto duplicate_capital_ships_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> FCapitalShipConfig* {
    auto* const copy{duplicate_level_config(config, outer)};
    return IsValid(copy) ? &copy->capital_ships : nullptr;
}

auto duplicate_capital_ship_fighters_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> FFighterConfig* {
    auto* const copy{duplicate_level_config(config, outer)};
    return IsValid(copy) ? &copy->fighters : nullptr;
}
}
