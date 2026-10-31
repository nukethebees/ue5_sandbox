#include <ioj/persistence/completion_publication.h>

#include <ioj/sim/levels/level_sim.h>

namespace ioj::persistence {
auto publish_player_completion(sim::LevelSim& simulation, ProfileManager* profiles, Timestamp date)
    -> std::optional<CompletionPublication> {
    auto const completion{simulation.take_player_team_completion()};
    if (!completion) {
        return std::nullopt;
    }

    auto const& metadata{simulation.get_result_metadata()};
    auto const& objectives{simulation.get_mission().objectives()};
    std::int32_t kills{};
    for (auto const count : simulation.get_entity_ledger()
                                .get_combat_telemetry()
                                .kills[static_cast<std::size_t>(completion->team)]) {
        kills += static_cast<std::int32_t>(count);
    }
    CompletionPublication publication{
        .record = {.date = date,
                   .level_name = metadata.level_id,
                   .team = completion->team,
                   .outcome = completion->outcome,
                   .objective_id = objectives.name(completion->objective).value,
                   .objective_outcome = completion->objective_outcome,
                   .kills = kills,
                   .time_seconds = static_cast<float>(sim::simulation_time(
                       completion->tick, simulation.get_clock().get_tick_period()))},
        .level_title = metadata.level_title,
        .par_time_seconds = metadata.par_time_seconds};
    if (profiles) {
        publication.save_result = profiles->append_score_record(publication.record);
    }
    return publication;
}
}
