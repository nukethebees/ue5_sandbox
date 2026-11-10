#pragma once
#include <ioj/duration.h>
#include <SpaceGame/levels/LevelOutcome.h>

#include <CoreMinimal.h>
namespace ioj::sim {
struct LevelSim;
}
struct FLevelCompletion {
    FName level_id;
    FString level_display_name;
    ioj::ELevelOutcome state{ioj::ELevelOutcome::Stopped};
    FString end_reason;
    FString winner;
    ioj::Duration elapsed_time;
    TOptional<int32> team_kills;
    TOptional<float> par_time_seconds;
    bool new_best_time{};
};
namespace ioj {
SPACEGAME_API auto make_level_completion(sim::LevelSim const& simulation) -> FLevelCompletion;
}
