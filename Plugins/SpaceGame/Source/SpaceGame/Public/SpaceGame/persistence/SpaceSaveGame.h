#pragma once

#include <SpaceGame/missions/ObjectiveOutcome.h>
#include <SpaceGame/missions/TeamOutcome.h>
#include <SpaceGamePresentation/entities/TestTeam.h>

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
    ETestTeam team{ETestTeam::White};
    UPROPERTY()
    ETeamOutcome outcome{ETeamOutcome::Defeat};
    UPROPERTY()
    FString objective_id;
    UPROPERTY()
    EObjectiveOutcome objective_outcome{EObjectiveOutcome::Failed};

    UPROPERTY()
    int32 kills{};

    UPROPERTY()
    float time_seconds{};
};

UCLASS()
class SPACEGAME_API USpaceSaveGame : public USaveGame {
    GENERATED_BODY()
  public:
    static constexpr int32 current_save_version{3};

    UPROPERTY()
    int32 save_version{current_save_version};

    UPROPERTY()
    TArray<FScoreRecord> score_records;
};

USTRUCT()
struct SPACEGAME_API FSaveProfileDebugSettings {
    GENERATED_BODY()

    UPROPERTY()
    bool unlock_all_missions{};

    UPROPERTY()
    bool start_levels_paused{};
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

    UPROPERTY()
    FSaveProfileDebugSettings debug_settings{};
};

USTRUCT()
struct SPACEGAME_API FSaveProfileIndexData {
    GENERATED_BODY()

    static constexpr int32 current_save_version{3};

    UPROPERTY()
    int32 save_version{current_save_version};

    UPROPERTY()
    FString active_profile_id{};

    UPROPERTY()
    TArray<FSaveProfileMetadata> profiles{};
};

USTRUCT()
struct SPACEGAME_API FSaveProfileResultsData {
    GENERATED_BODY()

    static constexpr int32 current_save_version{2};

    UPROPERTY()
    int32 save_version{current_save_version};

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
