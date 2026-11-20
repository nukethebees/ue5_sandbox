#include <ioj/profiles/level_score.h>

#include <ioj/sim/levels/level_sim.h>

namespace ioj::profiles {
auto completed_level(sim::LevelSim const& simulation) -> std::optional<CompletedLevel> {
    auto const result{simulation.result()};
    if (!result) {
        return std::nullopt;
    }
    auto const* player{simulation.player_ship_simulation()};
    auto const outcome{!result->winner
                           ? LevelOutcome::Stopped
                           : (!player || player->team == *result->winner ? LevelOutcome::Victory
                                                                         : LevelOutcome::Defeat)};
    return CompletedLevel{simulation.level_metadata(),
                          result->reason,
                          result->winner,
                          player ? std::optional{player->team} : std::nullopt,
                          outcome,
                          Duration::make(sim::simulation_time(result->terminal.tick,
                                                              simulation.clock().tick_period())),
                          player ? simulation.entity_ledger().count_kills(player->team) : 0};
}

auto make_level_score(CompletedLevel const& level, Timestamp date) -> std::optional<ScoreRecord> {
    if (!level.player_team || level.reason == sim::RunEndReason::WorldEnd ||
        level.reason == sim::RunEndReason::OrchestratorReset) {
        return std::nullopt;
    }
    return ScoreRecord{.date = date,
                       .level_name = level.metadata.id.value,
                       .team = *level.player_team,
                       .outcome = level.outcome,
                       .end_reason = level.reason,
                       .kills = level.player_team_kills,
                       .time_seconds = static_cast<float>(level.elapsed_time.seconds())};
}
}
