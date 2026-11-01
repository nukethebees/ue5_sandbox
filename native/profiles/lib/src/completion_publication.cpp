#include <ioj/profiles/completion_publication.h>

#include <ioj/profiles/profile_manager.h>
#include <ioj/sim/levels/level_sim.h>

namespace ioj::profiles {
auto publish_player_completion(sim::LevelSim& simulation, ProfileManager* profiles, Timestamp date)
    -> std::optional<CompletionPublication> {
    auto const completion{simulation.player_team_completion()};
    if (!completion) {
        return std::nullopt;
    }

    simulation.finalize_telemetry_run(completion->outcome == MissionOutcome::Victory
                                          ? sim::telemetry::RunEndReason::MissionSucceeded
                                          : sim::telemetry::RunEndReason::MissionFailed);

    auto const& metadata{simulation.level_metadata()};
    auto const& objectives{simulation.mission().objectives()};
    std::int32_t kills{};
    for (auto const count : simulation.entity_ledger()
                                .combat_telemetry()
                                .kills[static_cast<std::size_t>(completion->team)]) {
        kills += static_cast<std::int32_t>(count);
    }
    CompletionPublication publication{
        .record = {.date = date,
                   .level_name = metadata.id.value,
                   .team = completion->team,
                   .outcome = completion->outcome,
                   .objective_id = objectives.name(completion->triggering_objective).value,
                   .objective_outcome = completion->triggering_objective_outcome,
                   .kills = kills,
                   .time_seconds = static_cast<float>(
                       sim::simulation_time(completion->tick, simulation.clock().tick_period()))},
        .level_title = metadata.title,
        .par_time_seconds = metadata.par_time_seconds};
    if (profiles) {
        publication.save_result = profiles->append_score_record(publication.record);
    }
    return publication;
}
}
