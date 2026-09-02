#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "NebulaVolumeExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FNebulaVolumeSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume")
    FLinearColor shadow_colour{0.004f, 0.008f, 0.028f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume")
    FLinearColor emission_colour{0.05f, 0.28f, 0.55f, 1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume",
              meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float density{1.35f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume",
              meta = (ClampMin = "0.0", ClampMax = "8.0"))
    float extinction{1.1f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume", meta = (ClampMin = "0.0"))
    float emissive_strength{1.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume|Shape",
              meta = (ClampMin = "100.0", Units = "cm"))
    FVector extent{120000.0, 80000.0, 60000.0};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume|Structure",
              meta = (ClampMin = "100.0", Units = "cm"))
    float feature_size{15000.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume|Structure",
              meta = (ClampMin = "25.0", Units = "cm"))
    float detail_size{3500.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume|Structure")
    int32 seed{1337};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float flow_strength{0.32f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume", meta = (ClampMin = "0.0"))
    float drift_speed{0.025f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Nebula Volume",
              meta = (ClampMin = "4", ClampMax = "48"))
    int32 step_count{24};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API ANebulaVolumeExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    ANebulaVolumeExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nebula Volume|Quality")
    void set_low_quality();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nebula Volume|Quality")
    void set_balanced_quality();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nebula Volume|Quality")
    void set_high_quality();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Nebula Volume")
    void restart_animation();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nebula Volume")
    FNebulaVolumeSettings settings;
  private:
    void ensure_material();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Nebula Volume")
    TObjectPtr<USceneComponent> scene_root_;

    UPROPERTY(VisibleAnywhere, Category = "Nebula Volume")
    TObjectPtr<UStaticMeshComponent> volume_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> volume_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
