#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EnergyShieldExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FEnergyShieldSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Shield")
    FLinearColor base_colour{0.0f, 0.12f, 0.45f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Shield")
    FLinearColor edge_colour{0.05f, 0.95f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "1.0"))
    float hex_scale{18.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.005", ClampMax = "0.45"))
    float grid_width{0.08f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.1"))
    float fresnel_power{3.2f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.0"))
    float scan_speed{0.55f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.005", ClampMax = "0.5"))
    float scan_width{0.12f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float distortion{0.2f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.0"))
    float animation_speed{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{14.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Shield",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float opacity{0.55f};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AEnergyShieldExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AEnergyShieldExperimentActor();

    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Shield")
    FEnergyShieldSettings settings;
  private:
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Energy Shield")
    TObjectPtr<UStaticMeshComponent> shield_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> shield_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;
};
