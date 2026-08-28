#include "SimulationTestAssets.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <UObject/SoftObjectPath.h>
#include <UObject/SoftObjectPtr.h>
#include <UObject/UObjectGlobals.h>

namespace ml {
auto load_default_level_config() -> USpaceGameLevelConfig* {
    static TSoftObjectPtr<USpaceGameLevelConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_FT_soa_entities_LevelConfig."
                             "DA_FT_soa_entities_LevelConfig")}};

    return default_config.LoadSynchronous();
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
    return DuplicateObject<USpaceGameLevelConfig>(&config, &outer);
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
