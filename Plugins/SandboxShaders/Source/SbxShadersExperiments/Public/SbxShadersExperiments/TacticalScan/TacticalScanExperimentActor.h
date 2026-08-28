#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TacticalScanExperimentActor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FTacticalScanSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Scan")
    FLinearColor scan_colour{0.0f, 1.0f, 0.45f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "100.0"))
    float scan_range{450.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "0.0"))
    float scan_speed{0.22f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "1.0"))
    float scan_width{35.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "0.0"))
    float intensity{5.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "1.0"))
    float falloff{180.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Tactical Scan",
              meta = (ClampMin = "5.0"))
    float grid_scale{75.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Scan")
    FVector volume_extent{500.0, 500.0, 400.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Scan")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ATacticalScanExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ATacticalScanExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tactical Scan")
    void restart_scan();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical Scan")
    FTacticalScanSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Tactical Scan")
    TObjectPtr<UBoxComponent> scan_bounds_;

    UPROPERTY(VisibleAnywhere, Category = "Tactical Scan")
    TObjectPtr<UPostProcessComponent> post_process_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> scan_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
