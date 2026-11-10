#include <ioj/profiles/level_score.h>

#include <ioj/sim/levels/level_sim.h>

namespace ioj::profiles {
auto make_level_score(sim::LevelSim const& simulation, Timestamp date)
    -> std::optional<ScoreRecord> {
    auto const result{simulation.result()};
    auto const* player{simulation.player_ship_simulation()};
    if (!result || !player) {
        return std::nullopt;
    }
    auto const team{player->team};
    auto const outcome{
        !result->winner ? LevelOutcome::Stopped
                        : (*result->winner == team ? LevelOutcome::Victory : LevelOutcome::Defeat)};

    auto const& metadata{simulation.level_metadata()};
    return ScoreRecord{.date = date,
                       .level_name = metadata.id.value,
                       .team = team,
                       .outcome = outcome,
                       .end_reason = result->reason,
                       .kills = simulation.entity_ledger().count_kills(team),
                       .time_seconds = static_cast<float>(sim::simulation_time(
                           result->terminal.tick, simulation.clock().tick_period()))};
}
}
