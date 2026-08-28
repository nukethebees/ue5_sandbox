#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SpaceEnergyFieldExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FSpaceEnergyFieldSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "64", ClampMax = "2048"))
    int32 resolution{512};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.0"))
    float animation_speed{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.1"))
    float warp_scale{2.4f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float warp_strength{0.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float star_density{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.0"))
    float star_intensity{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Space Energy Field",
              meta = (ClampMin = "0.0"))
    float plasma_intensity{2.5f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Energy Field")
    FLinearColor colour_a{0.015f, 0.08f, 0.55f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Energy Field")
    FLinearColor colour_b{0.8f, 0.02f, 0.65f, 1.0f};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ASpaceEnergyFieldExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ASpaceEnergyFieldExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Space Energy Field")
    FSpaceEnergyFieldSettings settings;
  private:
    void ensure_render_resources();
    void submit_render(float time_seconds);

    UPROPERTY(VisibleAnywhere, Category = "Space Energy Field")
    TObjectPtr<UStaticMeshComponent> display_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> display_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> output_texture_;

    bool reported_missing_material_{false};
};
