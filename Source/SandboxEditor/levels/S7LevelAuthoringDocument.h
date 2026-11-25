#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Info.h>

#include "S7LevelAuthoringDocument.generated.h"

namespace ioj {
class USpaceGameLevelConfig;
}

USTRUCT()
struct FS7LevelEntityBinding {
    GENERATED_BODY()

    UPROPERTY()
    FString id;

    UPROPERTY()
    TObjectPtr<AActor> actor{nullptr};
};

USTRUCT()
struct FS7LevelCameraAuthoringData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Viewpoint")
    TArray<TObjectPtr<AActor>> targets{};

    UPROPERTY(EditAnywhere, Category = "Viewpoint")
    FVector offset_direction{-1.0, -1.0, 0.7};

    UPROPERTY(EditAnywhere, Category = "Viewpoint", meta = (ClampMin = "0.001"))
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
                         Tick,
                         TransformCommon,
                         WorldPartition))
class SANDBOXEDITOR_API AS7LevelAuthoringDocument final : public AInfo {
    GENERATED_BODY()
  public:
    AS7LevelAuthoringDocument();

    bool IsEditorOnly() const override { return true; }

    void PostLoad() override;
    void set_needs_export(bool value);
    void remember_editor_source();
    bool editor_source_matches() const;

    UPROPERTY()
    FString editor_source_hash;

    UPROPERTY(NonTransactional)
    bool needs_export{};

    UPROPERTY(VisibleAnywhere, Category = "Document")
    FString source_path{};

    UPROPERTY(
        VisibleAnywhere,
        Category = "Level",
        meta = (DisplayName = "Shared Level Assets",
                ToolTip = "Meshes, palettes, effects and collision classes shared by levels."))
    TObjectPtr<ioj::USpaceGameLevelConfig> level_config{nullptr};

    UPROPERTY()
    TArray<FS7LevelEntityBinding> entities{};

    UPROPERTY(EditAnywhere, Category = "Viewpoint")
    bool use_observer_camera{};

    UPROPERTY(EditAnywhere,
              Category = "Viewpoint",
              meta = (ShowOnlyInnerProperties, EditCondition = "use_observer_camera"))
    FS7LevelCameraAuthoringData camera{};
};
