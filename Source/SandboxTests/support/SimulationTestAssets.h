#pragma once

class USimulationConfig;
class UObject;
class UTestCapitalShipFightersConfig;
class UTestCapitalShipsConfig;
class UTestSimulationConfig;
namespace ml {
struct FSoftTestAssertions;
}

namespace ml {
auto load_default_simulation_config() -> USimulationConfig const*;
auto load_default_test_simulation_config() -> UTestSimulationConfig const*;
auto get_default_simulation_config(FSoftTestAssertions& checks) -> USimulationConfig const*;
auto get_default_test_config(FSoftTestAssertions& checks) -> UTestSimulationConfig const*;
auto duplicate_capital_ships_config(UTestSimulationConfig const& config, UObject& outer)
    -> UTestCapitalShipsConfig*;
auto duplicate_capital_ship_fighters_config(UTestSimulationConfig const& config, UObject& outer)
    -> UTestCapitalShipFightersConfig*;
}
