#pragma once

#include <SpaceGame/levels/LevelOutcome.h>

#include <CoreMinimal.h>

class UGameInstance;

namespace ioj::sim {
struct LevelSim;
}

struct FLevelCompletion {
    FString level_display_name{};
    ioj::ELevelOutcome state{ioj::ELevelOutcome::Defeat};
    FString end_reason{};
    bool persisted{false};
    TOptional<float> par_time_seconds{};
    bool new_best_time{false};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelCompleted, FLevelCompletion const&);

namespace ioj {
SPACEGAME_API auto record_level_completion(sim::LevelSim const& simulation,
                                           UGameInstance* game_instance)
    -> TOptional<FLevelCompletion>;
}
