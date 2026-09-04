#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "NebulaBackdropExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FNebulaBackdropSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop")
    FLinearColor shadow_colour{0.006f, 0.012f, 0.045f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop")
    FLinearColor emission_colour{0.08f, 0.34f, 0.62f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Backdrop",
              meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float density{1.15f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop", meta = (ClampMin = "0.0"))
    float brightness{2.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop", meta = (ClampMin = "0.05"))
    float texture_scale{1.4f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop")
    FVector2D texture_offset{0.0, 0.0};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Backdrop",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float parallax_strength{0.12f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Backdrop",
              meta = (ClampMin = "0.001", ClampMax = "0.5"))
    float edge_softness{0.16f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop", meta = (ClampMin = "0.0"))
    float drift_speed{0.018f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ANebulaBackdropExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ANebulaBackdropExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nebula Backdrop")
    void restart_animation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Backdrop")
    FNebulaBackdropSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Nebula Backdrop")
    TObjectPtr<UStaticMeshComponent> backdrop_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> backdrop_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
