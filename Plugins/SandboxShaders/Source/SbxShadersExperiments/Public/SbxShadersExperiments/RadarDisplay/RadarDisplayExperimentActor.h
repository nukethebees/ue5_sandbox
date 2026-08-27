#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "RadarDisplayExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FRadarContact {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Contact")
    FVector2D position{0.0, 0.0};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Contact",
              meta = (ClampMin = "0.005", ClampMax = "0.25"))
    float size{0.035f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Contact",
              meta = (ClampMin = "0.0"))
    float intensity{1.0f};
};

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FRadarDisplaySettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display")
    FLinearColor background_colour{0.001f, 0.018f, 0.012f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display")
    FLinearColor grid_colour{0.0f, 0.28f, 0.12f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display")
    FLinearColor sweep_colour{0.05f, 1.0f, 0.32f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display")
    FLinearColor contact_colour{1.0f, 0.55f, 0.05f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Display",
              meta = (ClampMin = "1.0", ClampMax = "12.0"))
    float ring_count{5.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Display",
              meta = (ClampMin = "0.0"))
    float sweep_speed{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Display",
              meta = (ClampMin = "0.005", ClampMax = "1.0"))
    float sweep_width{0.18f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Display",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float interference{0.22f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Radar Display",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{6.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display|Contacts")
    FRadarContact contact_a{FVector2D{-0.45, 0.2}, 0.04f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display|Contacts")
    FRadarContact contact_b{FVector2D{0.28, 0.5}, 0.03f, 0.8f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display|Contacts")
    FRadarContact contact_c{FVector2D{0.52, -0.22}, 0.05f, 1.2f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display|Contacts")
    FRadarContact contact_d{FVector2D{-0.12, -0.58}, 0.025f, 0.7f};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ARadarDisplayExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ARadarDisplayExperimentActor();

    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar Display")
    FRadarDisplaySettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Radar Display")
    TObjectPtr<UStaticMeshComponent> display_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> display_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;
};
