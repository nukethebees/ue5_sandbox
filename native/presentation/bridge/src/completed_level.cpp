#include <ioj/bridge/completed_level.h>

#include <ioj/sim/levels/level_sim.h>

namespace ioj::bridge {
auto completed_level(sim::LevelSim const& simulation, sim::LevelResult const& result)
    -> profiles::CompletedLevel {
    auto const* player{simulation.player_ship_simulation()};
    auto const outcome{!result.winner
                           ? LevelOutcome::Stopped
                           : (!player || player->team == *result.winner ? LevelOutcome::Victory
                                                                        : LevelOutcome::Defeat)};
    return profiles::CompletedLevel{
        simulation.level_metadata(),
        result.reason,
        result.winner,
        player ? std::optional{player->team} : std::nullopt,
        outcome,
        Duration::make(
            sim::simulation_time(result.terminal.tick, simulation.clock().tick_period())),
        player ? simulation.entity_ledger().count_kills(player->team) : 0};
}

}
