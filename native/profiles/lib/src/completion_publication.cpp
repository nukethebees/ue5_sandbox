#include <ioj/profiles/completion_publication.h>

#include <ioj/profiles/profile_manager.h>
#include <ioj/sim/levels/level_sim.h>

namespace ioj::profiles {
auto publish_player_completion(sim::LevelSim& simulation, ProfileManager* profiles, Timestamp date)
    -> std::optional<CompletionPublication> {
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
    std::int32_t kills{};
    auto const& team_kills{simulation.entity_ledger().combat_telemetry().kills};
    for (auto const type : EnumTraits<sim::EntityType>::values) {
        kills += static_cast<std::int32_t>(team_kills[team, type]);
    }
    CompletionPublication publication{
        .record = {.date = date,
                   .level_name = metadata.id.value,
                   .team = team,
                   .outcome = outcome,
                   .end_reason = result->reason,
                   .kills = kills,
                   .time_seconds = static_cast<float>(sim::simulation_time(
                       result->terminal.tick, simulation.clock().tick_period()))},
        .level_title = metadata.title,
        .par_time_seconds = metadata.par_time_seconds};
    if (profiles) {
        publication.save_result = profiles->append_score_record(publication.record);
    }
    return publication;
}
}
