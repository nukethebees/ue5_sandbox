#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ConstructionSpawnExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FConstructionSpawnSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float progress{0.58f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "0.005", ClampMax = "0.35"))
    float edge_width{0.075f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "1.0"))
    float grid_scale{9.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "0.1"))
    float noise_scale{2.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "0.0"))
    float animation_speed{0.16f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Construction Spawn",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{13.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction Spawn")
    FLinearColor base_colour{0.015f, 0.10f, 0.16f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction Spawn")
    FLinearColor edge_colour{0.05f, 0.95f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction Spawn")
    bool auto_animate{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction Spawn")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AConstructionSpawnExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AConstructionSpawnExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Construction Spawn")
    void reset_effect();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Construction Spawn")
    void complete_effect();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Construction Spawn")
    void restart_animation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction Spawn")
    FConstructionSpawnSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Construction Spawn")
    TObjectPtr<UStaticMeshComponent> construction_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> construction_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animated_progress_{0.58f};
    float animation_time_{0.0f};
};
