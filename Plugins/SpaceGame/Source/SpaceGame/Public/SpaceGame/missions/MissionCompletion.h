#pragma once

#include <CoreMinimal.h>
#include <SpaceGameSimulation/missions/TestMissionFailReason.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>

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
