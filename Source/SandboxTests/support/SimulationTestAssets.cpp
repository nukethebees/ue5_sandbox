#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <UObject/SoftObjectPath.h>
#include <UObject/SoftObjectPtr.h>
#include <UObject/UObjectGlobals.h>

namespace ml {
auto load_default_simulation_config() -> USimulationConfig const* {
    static TSoftObjectPtr<USimulationConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_default_simulation_config."
                             "DA_default_simulation_config")}};

    return default_config.LoadSynchronous();
}

auto load_default_test_simulation_config() -> UTestSimulationConfig const* {
    static TSoftObjectPtr<UTestSimulationConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_default_test_simulation_config."
                             "DA_default_test_simulation_config")}};

    return default_config.LoadSynchronous();
}
auto get_default_simulation_config(FSoftTestAssertions& checks) -> USimulationConfig const* {
    auto const* const config{load_default_simulation_config()};
    if (!checks.not_nullptr(const_cast<USimulationConfig*>(config),
                            TEXT("Default simulation config loads"))) {
        return nullptr;
    }
    if (!checks.is_true(config->is_valid(), TEXT("Default simulation config is valid"))) {
        return nullptr;
    }
    return config;
}
auto get_default_test_config(FSoftTestAssertions& checks) -> UTestSimulationConfig const* {
    auto const* const config{load_default_test_simulation_config()};
    if (!checks.not_nullptr(const_cast<UTestSimulationConfig*>(config),
                            TEXT("Default test simulation config loads"))) {
        return nullptr;
    }
    if (!checks.is_true(config->is_valid(), TEXT("Default test simulation config is valid"))) {
        return nullptr;
    }
    return config;
}

auto duplicate_capital_ships_config(UTestSimulationConfig const& config, UObject& outer)
    -> UTestCapitalShipsConfig* {
    auto* const simulation_config{config.simulation_config.Get()};
    auto* const capital_config{
        IsValid(simulation_config) ? simulation_config->capital_ships_config.Get() : nullptr};
    return IsValid(capital_config)
             ? DuplicateObject<UTestCapitalShipsConfig>(capital_config, &outer)
             : nullptr;
}

auto duplicate_capital_ship_fighters_config(UTestSimulationConfig const& config, UObject& outer)
    -> UTestCapitalShipFightersConfig* {
    auto* const simulation_config{config.simulation_config.Get()};
    auto* const fighter_config{IsValid(simulation_config)
                                   ? simulation_config->capital_ship_fighters_config.Get()
                                   : nullptr};
    return IsValid(fighter_config)
             ? DuplicateObject<UTestCapitalShipFightersConfig>(fighter_config, &outer)
             : nullptr;
}
}
