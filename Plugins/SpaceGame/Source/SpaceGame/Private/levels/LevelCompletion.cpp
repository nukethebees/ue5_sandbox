#include <SpaceGame/levels/LevelCompletion.h>

#include <ioj/profiles/completion_publication.h>
#include <SpaceGame/levels/LevelOutcomeConversion.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Engine/GameInstance.h>

namespace ioj {
void publish_level_completion(sim::LevelSim& simulation,
                              UGameInstance* game_instance,
                              FOnLevelCompleted& completed) {
    auto* const saves{USpaceSaveSubsystem::get(game_instance)};
    auto const publication{profiles::publish_player_completion(
        simulation, IsValid(saves) ? &saves->profiles() : nullptr, profiles::current_timestamp())};
    if (!publication) {
        return;
    }
    if (!publication->save_result.persisted) {
        UE_LOG(LogSandbox,
               Error,
               TEXT("Cannot persist mission result: native profiles unavailable or save failed"));
    }

    TOptional<float> par_time_seconds;
    if (publication->par_time_seconds) {
        par_time_seconds = *publication->par_time_seconds;
    }
    auto const& record{publication->record};
    completed.Broadcast({.level_display_name = UTF8_TO_TCHAR(publication->level_title.c_str()),
                         .state = to_unreal(record.outcome),
                         .end_reason = UTF8_TO_TCHAR(record.end_reason.c_str()),
                         .persisted = publication->save_result.persisted,
                         .par_time_seconds = par_time_seconds,
                         .new_best_time = publication->save_result.new_best_time});
}
}
