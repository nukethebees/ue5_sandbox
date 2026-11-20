#pragma once
#include <ioj/duration.h>
#include <ioj/kill_count.h>
#include <SpaceGame/levels/LevelOutcome.h>

#include <CoreMinimal.h>
namespace ioj::profiles {
struct CompletedLevel;
}
struct FLevelCompletion {
    FName level_id;
    FString level_display_name;
    ioj::ELevelOutcome state{ioj::ELevelOutcome::Stopped};
    FString end_reason;
    FString winner;
    ioj::Duration elapsed_time;
    TOptional<ioj::KillCount> team_kills;
    TOptional<float> par_time_seconds;
    bool new_best_time{};
};
namespace ioj {
SPACEGAME_API auto make_level_completion(profiles::CompletedLevel const& level) -> FLevelCompletion;
}
