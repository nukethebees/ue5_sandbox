#pragma once

#include <SpaceGame/missions/TestMissionFailReason.h>
#include <SpaceGame/missions/TestMissionState.h>

#include <CoreMinimal.h>

struct FTestMissionCompletion {
    FName level_id{NAME_None};
    FString level_display_name{};
    ETestMissionState state{ETestMissionState::NotStarted};
    ETestMissionFailReason fail_reason{ETestMissionFailReason::None};
    bool persisted{false};
    TOptional<float> par_time_seconds{};
    bool new_best_time{false};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTestMissionCompleted, FTestMissionCompletion const&);
