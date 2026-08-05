#pragma once

class USimulationConfig;
class UTestSimulationConfig;

namespace ml {
auto load_default_simulation_config() -> USimulationConfig const*;
auto load_default_test_simulation_config() -> UTestSimulationConfig const*;
}
