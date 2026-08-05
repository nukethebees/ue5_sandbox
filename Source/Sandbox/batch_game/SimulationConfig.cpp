#include "SimulationConfig.h"

#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestLasersConfig.h>
#include <Sandbox/batch_game/TestSpaceShipData.h>
#include <Sandbox/batch_game/TestStaticTurretsConfig.h>
#include <Sandbox/batch_game/TestTubeSpinnersConfig.h>


auto USimulationConfig::is_valid() const noexcept -> bool {
    return IsValid(player_ship_config) && IsValid(lasers_config) &&
           IsValid(capital_ships_config) && IsValid(capital_ship_fighters_config) &&
           IsValid(static_turrets_config) && IsValid(tube_spinners_config);
}
