#include <SpaceGame/missions/MissionCompletion.h>

#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/missions/ObjectiveOutcomeConversion.h>
#include <SpaceGame/missions/TeamOutcomeConversion.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Engine/GameInstance.h>
#include <Misc/DateTime.h>

namespace ioj {
void publish_mission_completion(sim::LevelSim& simulation,
                                UGameInstance* game_instance,
                                FOnMissionCompleted& completed) {
    auto result{simulation.take_player_team_completion()};
    if (!result.has_value()) {
        return;
    }

    auto const& metadata{simulation.get_result_metadata()};
    auto const& objective_name{simulation.get_mission().objectives().name(result->objective).value};
    auto const elapsed_seconds{static_cast<float>(
        ioj::sim::simulation_time(result->tick, simulation.get_clock().get_tick_period()))};

    TOptional<float> par_time_seconds;
    if (metadata.par_time_seconds) {
        par_time_seconds = *metadata.par_time_seconds;
    }
    bool persisted{};
    bool new_best_time{};
    auto* saves{USpaceSaveSubsystem::get(game_instance)};
    if (IsValid(saves)) {
        if (result->outcome == ioj::TeamOutcome::Victory) {
            auto const previous_progress{saves->get_level_progress(
                ioj::FLevelId{FName{UTF8_TO_TCHAR(metadata.level_id.c_str())}})};
            new_best_time = previous_progress.best_completion_time_seconds < 0.0f ||
                            elapsed_seconds < previous_progress.best_completion_time_seconds;
        }

        int32 team_kills{};
        for (auto const kills : simulation.get_entity_ledger()
                                    .get_combat_telemetry()
                                    .kills[static_cast<int32>(result->team)]) {
            team_kills += static_cast<int32>(kills);
        }
        FScoreRecord const record{
            .date = FDateTime::Now(),
            .level_name = FName{UTF8_TO_TCHAR(metadata.level_id.c_str())},
            .team = ioj::to_unreal(result->team),
            .outcome = ioj::to_unreal(result->outcome),
            .objective_id = UTF8_TO_TCHAR(objective_name.c_str()),
            .objective_outcome = ioj::to_unreal(result->objective_outcome),
            .kills = team_kills,
            .time_seconds = elapsed_seconds,
        };
        persisted = saves->save_score_record(record);
        new_best_time = persisted && new_best_time;
    } else {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot persist mission result: save subsystem is unavailable"));
    }

    completed.Broadcast({.level_display_name = UTF8_TO_TCHAR(metadata.level_title.c_str()),
                         .state = ioj::to_unreal(result->outcome),
                         .objective_id = UTF8_TO_TCHAR(objective_name.c_str()),
                         .persisted = persisted,
                         .par_time_seconds = par_time_seconds,
                         .new_best_time = new_best_time});
}
}
