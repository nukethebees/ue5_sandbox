#pragma once

#include <SpaceGame/missions/TestMissionFailReason.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>

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
    ETestMissionFailReason fail_reason{ETestMissionFailReason::None};

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
class SPACEGAME_API USpaceSaveGame : public USaveGame {
    GENERATED_BODY()
  public:
    static constexpr int32 current_save_version{2};

    UPROPERTY()
    int32 save_version{current_save_version};

    UPROPERTY()
    TArray<FScoreRecord> score_records;
};

USTRUCT()
struct SPACEGAME_API FSaveProfileMetadata {
    GENERATED_BODY()

    UPROPERTY()
    FString profile_id{};

    UPROPERTY()
    FString display_name{};

    UPROPERTY()
    FDateTime created_at{};

    UPROPERTY()
    FDateTime last_played_at{};

    UPROPERTY()
    float total_simulation_duration_seconds{};

    UPROPERTY()
    int32 total_kills{};

    UPROPERTY()
    int32 outcome_count{};
};

USTRUCT()
struct SPACEGAME_API FSaveProfileIndexData {
    GENERATED_BODY()

    UPROPERTY()
    int32 save_version{1};

    UPROPERTY()
    FString active_profile_id{};

    UPROPERTY()
    TArray<FSaveProfileMetadata> profiles{};
};

USTRUCT()
struct SPACEGAME_API FSaveProfileResultsData {
    GENERATED_BODY()

    UPROPERTY()
    int32 save_version{1};

    UPROPERTY()
    TArray<FScoreRecord> score_records{};
};

UCLASS()
class SPACEGAME_API USpaceSaveProfileIndexSaveGame : public USaveGame {
    GENERATED_BODY()
  public:
    UPROPERTY()
    FSaveProfileIndexData data{};
};

UCLASS()
class SPACEGAME_API USpaceSaveProfileResultsSaveGame : public USaveGame {
    GENERATED_BODY()
  public:
    UPROPERTY()
    FSaveProfileResultsData data{};
};
