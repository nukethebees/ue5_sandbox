#include "TestSimulationConfig.h"

#include <Sandbox/batch_game/SimulationConfig.h>

auto UTestSimulationConfig::is_valid() const noexcept -> bool {
    return IsValid(simulation_config) && simulation_config->is_valid() &&
           IsValid(player_controller_class) &&
           IsValid(actor_classes.lasers_class) && IsValid(actor_classes.capital_ships_class) &&
           IsValid(actor_classes.capital_ship_fighters_class) &&
           IsValid(actor_classes.turrets_class) && IsValid(actor_classes.spinners_class) &&
           IsValid(actor_classes.entity_registry_class) &&
           IsValid(actor_classes.mission_manager_class) &&
           IsValid(actor_classes.niagara_spawner_class);
}
