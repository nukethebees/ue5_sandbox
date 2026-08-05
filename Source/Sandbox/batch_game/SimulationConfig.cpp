#include "SimulationConfig.h"

#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestLasersConfig.h>
#include <Sandbox/batch_game/TestSpaceShipData.h>
#include <Sandbox/batch_game/TestStaticTurretsConfig.h>
#include <Sandbox/batch_game/TestTubeSpinnersConfig.h>

#include <UObject/UObjectGlobals.h>

namespace {
template <typename T>
auto duplicate_config(T const* const config, UObject& outer) -> T* {
    if (!IsValid(config)) { return nullptr; }

    return DuplicateObject<T>(config, &outer);
}
}

auto USimulationConfig::is_valid() const noexcept -> bool {
    return IsValid(player_ship_config) && IsValid(lasers_config) && IsValid(capital_ships_config) &&
           IsValid(capital_ship_fighters_config) && IsValid(static_turrets_config) &&
           IsValid(tube_spinners_config);
}

auto USimulationConfig::deep_copy(UObject* const outer) const -> USimulationConfig* {
    auto* const copy_outer{IsValid(outer) ? outer : GetTransientPackage()};
    auto* const copy{DuplicateObject<USimulationConfig>(this, copy_outer)};
    check(IsValid(copy));

    copy->player_ship_config = duplicate_config(player_ship_config.Get(), *copy);
    copy->lasers_config = duplicate_config(lasers_config.Get(), *copy);
    copy->capital_ships_config = duplicate_config(capital_ships_config.Get(), *copy);
    copy->capital_ship_fighters_config =
        duplicate_config(capital_ship_fighters_config.Get(), *copy);
    copy->static_turrets_config = duplicate_config(static_turrets_config.Get(), *copy);
    copy->tube_spinners_config = duplicate_config(tube_spinners_config.Get(), *copy);

    return copy;
}
