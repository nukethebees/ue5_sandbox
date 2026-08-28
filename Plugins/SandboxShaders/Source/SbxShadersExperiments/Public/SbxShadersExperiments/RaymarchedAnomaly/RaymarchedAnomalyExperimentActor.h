#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "RaymarchedAnomalyExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FRaymarchedAnomalySettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarched Anomaly")
    FLinearColor colour_a{0.02f, 0.15f, 0.9f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarched Anomaly")
    FLinearColor colour_b{1.0f, 0.04f, 0.45f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "8", ClampMax = "96"))
    int32 step_count{64};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "0.1"))
    float scale{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float deformation{0.42f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "0.0"))
    float animation_speed{0.45f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "0.1"))
    float noise_frequency{3.2f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "0.0"))
    float emissive_strength{7.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Raymarched Anomaly",
              meta = (ClampMin = "1.0", ClampMax = "20.0"))
    float max_distance{7.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarched Anomaly")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ARaymarchedAnomalyExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ARaymarchedAnomalyExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Raymarched Anomaly")
    void restart_animation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarched Anomaly")
    FRaymarchedAnomalySettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Raymarched Anomaly")
    TObjectPtr<UStaticMeshComponent> display_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> display_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
