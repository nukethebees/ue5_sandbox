#pragma once

#include <CoreTypes.h>

class USimulationConfig;
class UTestSimulationConfig;
namespace ml {
struct FSoftTestAssertions;
}

namespace ml {
struct FLevelTestConfigPaths {
    static constexpr TCHAR capital_fighter_handles_capital_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/")
            TEXT("FuncT_capital_fighter_handles/DA_TestCapitalShips_capital_fighter_handles")};
    static constexpr TCHAR capital_fighter_handles_fighter_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/") TEXT(
            "FuncT_capital_fighter_handles/DA_TestCapitalShipFighters_capital_fighter_handles")};
    static constexpr TCHAR entity_interface_capital_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/")
            TEXT("FuncT_simple_batch/DA_TestCapitalShips")};
    static constexpr TCHAR entity_registry_capital_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/")
            TEXT("FuncT_entity_registry/DA_TestCapitalShips_entity_registry")};
    static constexpr TCHAR fighter_attack_fighter_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/")
            TEXT("FuncT_fighter_attack/DA_TestCapitalShipFighters")};
    static constexpr TCHAR player_ship_vs_capital_fighter_config[]{
        TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests/")
            TEXT("FuncT_player_ship_vs_capital/DA_TestCapitalShipFighters")};
};

auto load_default_simulation_config() -> USimulationConfig const*;
auto load_default_test_simulation_config() -> UTestSimulationConfig const*;
auto get_default_simulation_config(FSoftTestAssertions& checks) -> USimulationConfig const*;
auto get_default_test_config(FSoftTestAssertions& checks) -> UTestSimulationConfig const*;
}
