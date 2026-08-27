#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VertexRippleExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FVertexRippleSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertex Ripple")
    FVector2D origin_uv{0.5, 0.5};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "0.0"))
    float amplitude{85.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "0.1"))
    float wave_speed{0.32f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "1.0"))
    float wavelength{9.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "0.005", ClampMax = "1.0"))
    float wave_width{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "0.0"))
    float falloff{1.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertex Ripple")
    FLinearColor base_colour{0.005f, 0.06f, 0.09f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertex Ripple")
    FLinearColor crest_colour{0.0f, 0.9f, 0.65f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Vertex Ripple",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{8.0f};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AVertexRippleExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AVertexRippleExperimentActor();

    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertex Ripple")
    FVertexRippleSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Vertex Ripple")
    TObjectPtr<UStaticMeshComponent> surface_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> surface_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;
};
