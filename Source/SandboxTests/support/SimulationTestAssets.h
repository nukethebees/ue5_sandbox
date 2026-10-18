#pragma once

class UObject;
class USpaceGameLevelConfig;
struct FCapitalShipConfig;
struct FFighterConfig;
namespace ioj {
struct FSoftTestAssertions;
}

namespace ioj {
auto load_default_level_config() -> USpaceGameLevelConfig*;
auto get_default_level_config(FSoftTestAssertions& checks) -> USpaceGameLevelConfig const*;
auto duplicate_level_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> USpaceGameLevelConfig*;
auto duplicate_capital_ships_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> FCapitalShipConfig*;
auto duplicate_capital_ship_fighters_config(USpaceGameLevelConfig const& config, UObject& outer)
    -> FFighterConfig*;
}
