#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PlanetAtmosphereExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FPlanetAtmosphereSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FVector sun_direction{0.35, -0.45, 0.82};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FLinearColor surface_day_colour{0.04f, 0.16f, 0.24f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FLinearColor surface_night_colour{0.002f, 0.006f, 0.018f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FLinearColor atmosphere_colour{0.05f, 0.45f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Planet Atmosphere",
              meta = (ClampMin = "0.01", ClampMax = "8.0"))
    float density{2.2f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Planet Atmosphere",
              meta = (ClampMin = "0.5", ClampMax = "12.0"))
    float limb_power{3.4f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Planet Atmosphere",
              meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float scale_height{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Planet Atmosphere",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{8.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Planet Atmosphere",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float night_floor{0.08f};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API APlanetAtmosphereExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    APlanetAtmosphereExperimentActor();

    void OnConstruction(FTransform const& transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Planet Atmosphere")
    void reset_sun_direction();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FPlanetAtmosphereSettings settings;
  private:
    void ensure_materials();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Planet Atmosphere")
    TObjectPtr<USceneComponent> root_;

    UPROPERTY(VisibleAnywhere, Category = "Planet Atmosphere")
    TObjectPtr<UStaticMeshComponent> surface_mesh_;

    UPROPERTY(VisibleAnywhere, Category = "Planet Atmosphere")
    TObjectPtr<UStaticMeshComponent> atmosphere_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> surface_material_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> atmosphere_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> surface_instance_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> atmosphere_instance_;
};
