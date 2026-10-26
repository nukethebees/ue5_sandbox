#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EngineExhaustExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FEngineExhaustSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float throttle{0.82f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.0"))
    float thrust_intensity{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "10.0"))
    float exhaust_length{520.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "1.0"))
    float radius{105.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.01"))
    float display_scale{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.0"))
    float flow_speed{1.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float turbulence{0.65f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Engine Exhaust",
              meta = (ClampMin = "0.0"))
    float emissive_intensity{14.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Exhaust")
    FLinearColor core_colour{0.5f, 0.85f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Exhaust")
    FLinearColor tail_colour{0.05f, 0.15f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Exhaust")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AEngineExhaustExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AEngineExhaustExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Engine Exhaust")
    void set_idle();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Engine Exhaust")
    void set_half_throttle();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Engine Exhaust")
    void set_full_throttle();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Exhaust")
    FEngineExhaustSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Engine Exhaust")
    TObjectPtr<UStaticMeshComponent> exhaust_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> exhaust_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
