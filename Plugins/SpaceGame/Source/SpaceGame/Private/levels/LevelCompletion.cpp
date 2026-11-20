#include <SpaceGame/levels/LevelCompletion.h>

#include <ioj/profiles/level_score.h>
#include <SpaceGame/levels/LevelOutcomeConversion.h>

#include <SandboxCoreEngine/strings.h>
namespace ioj {
auto make_level_completion(profiles::CompletedLevel const& level) -> FLevelCompletion {
    auto const& metadata{level.metadata};
    FLevelCompletion completion{.level_id = FName{*to_fstring(metadata.id.value)},
                                .level_display_name = to_fstring(metadata.title),
                                .end_reason = to_fstring(sim::to_serialized_string(level.reason)),
                                .elapsed_time = level.elapsed_time};
    completion.state = to_unreal(level.outcome);
    if (level.winner) {
        completion.winner = to_fstring(to_serialized_string(*level.winner));
    }
    if (metadata.par_time_seconds) {
        completion.par_time_seconds = *metadata.par_time_seconds;
    }
    return completion;
}
}
