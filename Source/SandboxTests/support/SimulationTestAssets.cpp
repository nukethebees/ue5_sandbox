#include "SimulationTestAssets.h"

#include <ioj/levels/authoring/definition_reader.h>
#include <SandboxTests/SandboxTestLogCategories.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Misc/Paths.h>
#include <UObject/Package.h>
#include <UObject/SoftObjectPath.h>
#include <UObject/SoftObjectPtr.h>
#include <UObject/StrongObjectPtr.h>
#include <UObject/UObjectGlobals.h>

namespace ioj {
auto default_test_settings() -> settings::Settings const& {
    static auto const settings{[] {
        auto const root{
            std::filesystem::path{*FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))}};
        auto result{levels::authoring::DefinitionReader{root}.read_settings_file(
            root / "Settings/base.scm")};
        check(result);
        return std::move(*result);
    }()};
    return settings;
}
auto load_default_level_config() -> USpaceGameLevelConfig* {
    static TSoftObjectPtr<USpaceGameLevelConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_FT_soa_entities_LevelConfig."
                             "DA_FT_soa_entities_LevelConfig")}};

    auto* const config{default_config.LoadSynchronous()};
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
