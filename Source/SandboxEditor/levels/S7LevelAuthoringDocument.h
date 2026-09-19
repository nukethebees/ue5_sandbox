#pragma once

#include <SpaceGameSimulation/missions/TestMissionMode.h>

#include <CoreMinimal.h>
#include <GameFramework/Info.h>

#include "S7LevelAuthoringDocument.generated.h"

class USpaceGameLevelConfig;

USTRUCT()
struct FS7LevelEntityBinding {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Level Authoring")
    FName id{NAME_None};

    UPROPERTY(VisibleAnywhere, Category = "Level Authoring")
    TObjectPtr<AActor> actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.0", Units = "s"))
    double spawn_time_seconds{};
};

USTRUCT()
struct FS7LevelCameraAuthoringData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Level Authoring")
    TArray<TObjectPtr<AActor>> targets{};

    UPROPERTY(EditAnywhere, Category = "Level Authoring")
    FVector offset_direction{-1.0, -1.0, 0.7};

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.001"))
    double distance{100000.0};
};

USTRUCT()
struct FS7LevelMissionAuthoringData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Mission")
    ETestMissionMode mode{ETestMissionMode::None};

    UPROPERTY(EditAnywhere,
              Category = "Mission",
              meta = (ClampMin = "0.001", EditCondition = "mode != ETestMissionMode::KillEnemies"))
    float time_limit_seconds{60.0f};

    UPROPERTY(
        EditAnywhere,
        Category = "Mission",
        meta = (EditCondition =
                    "mode != ETestMissionMode::SurviveTime && mode != ETestMissionMode::None"))
    bool use_explicit_kill_count{};

    UPROPERTY(EditAnywhere,
              Category = "Mission",
              meta = (ClampMin = "1", EditCondition = "use_explicit_kill_count"))
    int32 kill_count{1};

    UPROPERTY(EditAnywhere, Category = "Mission")
    TArray<TObjectPtr<AActor>> heroes{};

    UPROPERTY(EditAnywhere, Category = "Mission")
    TArray<TObjectPtr<AActor>> must_survive{};

    UPROPERTY(EditAnywhere, Category = "Mission")
    TArray<TObjectPtr<AActor>> required_kills{};
};

UCLASS(NotBlueprintable,
       HideCategories = (Actor,
                         Collision,
                         Cooking,
                         DataLayers,
                         HLOD,
                         Input,
                         LevelInstance,
                         Networking,
                         Physics,
                         Replication,
                         Rendering,
                         WorldPartition))
class SANDBOXEDITOR_API AS7LevelAuthoringDocument final : public AInfo {
    GENERATED_BODY()
  public:
    AS7LevelAuthoringDocument();

    bool IsEditorOnly() const override { return true; }

    UPROPERTY(EditAnywhere, Category = "Document")
    FString source_path{};

    UPROPERTY(VisibleAnywhere, Category = "Document")
    FString synchronized_source_hash{};

    UPROPERTY(VisibleAnywhere, Category = "Document")
    FString synchronized_scene_hash{};

    UPROPERTY(EditAnywhere, Category = "Level")
    FName level_id{NAME_None};

    UPROPERTY(EditAnywhere, Category = "Level")
    FString title{};

    UPROPERTY(EditAnywhere, Category = "Level", meta = (MultiLine = "true"))
    FString description{};

    UPROPERTY(EditAnywhere, Category = "Level")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    UPROPERTY(EditAnywhere, EditFixedSize, Category = "Entities", meta = (TitleProperty = "id"))
    TArray<FS7LevelEntityBinding> entities{};

    UPROPERTY(EditAnywhere, Category = "Viewpoint")
    bool use_observer_camera{};

    UPROPERTY(EditAnywhere,
              Category = "Viewpoint",
              meta = (ShowOnlyInnerProperties, EditCondition = "use_observer_camera"))
    FS7LevelCameraAuthoringData camera{};

    UPROPERTY(EditAnywhere, Category = "Mission", meta = (ShowOnlyInnerProperties))
    FS7LevelMissionAuthoringData mission{};
};
