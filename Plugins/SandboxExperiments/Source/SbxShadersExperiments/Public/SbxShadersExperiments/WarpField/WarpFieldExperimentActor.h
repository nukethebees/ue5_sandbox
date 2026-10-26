#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "WarpFieldExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FWarpFieldSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field")
    FLinearColor core_colour{0.04f, 0.1f, 0.35f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field")
    FLinearColor edge_colour{0.35f, 0.05f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Warp Field",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float distortion_strength{0.22f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field", meta = (ClampMin = "0.1"))
    float noise_scale{4.5f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field", meta = (ClampMin = "0.0"))
    float noise_speed{0.35f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field", meta = (ClampMin = "0.0"))
    float pulse_frequency{1.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field", meta = (ClampMin = "0.0"))
    float edge_intensity{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Warp Field",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float opacity{0.28f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AWarpFieldExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AWarpFieldExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Warp Field")
    void restart_animation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp Field")
    FWarpFieldSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Warp Field")
    TObjectPtr<UStaticMeshComponent> field_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> field_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
