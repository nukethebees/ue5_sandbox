#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

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

    UPROPERTY()
    FString synchronized_source{};

    UPROPERTY()
    FString synchronized_base_settings_source{};

    UPROPERTY()
    FString evaluated_level_source{};

    UPROPERTY()
    FString evaluated_base_settings_source{};

    UPROPERTY()
    FString synchronized_scene_source{};

    UPROPERTY()
    FName level_id{NAME_None};

    UPROPERTY()
    FString title{};

    UPROPERTY()
    FString description{};

    UPROPERTY(VisibleAnywhere,
              Category = "Level",
              meta = (ToolTip = "Shared canonical S7 runtime resources"))
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    UPROPERTY(EditAnywhere, EditFixedSize, Category = "Entities", meta = (TitleProperty = "id"))
    TArray<FS7LevelEntityBinding> entities{};

    UPROPERTY(EditAnywhere, Category = "Viewpoint")
    bool use_observer_camera{};

    UPROPERTY(EditAnywhere,
              Category = "Viewpoint",
              meta = (ShowOnlyInnerProperties, EditCondition = "use_observer_camera"))
    FS7LevelCameraAuthoringData camera{};
};
