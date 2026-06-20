#pragma once

#include <Sandbox/batch_game/TestMissionMode.h>
#include <Sandbox/batch_game/TestMissionState.h>

#include <CoreMinimal.h>
#include <GameFramework/SaveGame.h>

#include "SpaceSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FScoreRecord {
    GENERATED_BODY()

    UPROPERTY()
    FDateTime date{};

    UPROPERTY()
    FName level_name{};

    UPROPERTY()
    ETestMissionMode mission_mode{ETestMissionMode::None};

    UPROPERTY()
    ETestMissionState end_state{ETestMissionState::NotStarted};

    UPROPERTY()
    int32 kills{};

    UPROPERTY()
    float time_seconds{};

    // Objectives
    UPROPERTY()
    int32 target_kills{-1};

    UPROPERTY()
    float target_completion_time{-1.f};
};

UCLASS()
class SANDBOX_API USpaceSaveGame : public USaveGame {
    GENERATED_BODY()
  public:
    static constexpr int32 current_save_version{1};

    UPROPERTY()
    int32 save_version{current_save_version};

    UPROPERTY()
    TArray<FScoreRecord> score_records;
};
