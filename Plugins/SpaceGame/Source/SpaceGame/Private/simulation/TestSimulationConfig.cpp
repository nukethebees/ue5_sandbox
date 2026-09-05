#include "SpaceGame/simulation/TestSimulationConfig.h"

#include <SpaceGame/simulation/SimulationConfig.h>

auto UTestSimulationConfig::is_valid() const noexcept -> bool {
    return IsValid(simulation_config) && simulation_config->is_valid() &&
           IsValid(player_controller_class) && IsValid(actor_classes.capital_ship_proxy_class);
}
