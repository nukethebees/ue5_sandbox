#pragma once

#include <SpaceGame/missions/TeamOutcome.h>

#include <CoreMinimal.h>

struct FMissionCompletion {
    FString level_display_name{};
    ETeamOutcome state{ETeamOutcome::Defeat};
    FString objective_id{};
    bool persisted{false};
    TOptional<float> par_time_seconds{};
    bool new_best_time{false};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionCompleted, FMissionCompletion const&);
