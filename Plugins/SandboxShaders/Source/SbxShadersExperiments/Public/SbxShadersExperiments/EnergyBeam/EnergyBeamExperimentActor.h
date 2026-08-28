#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EnergyBeamExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FEnergyBeamSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    FVector source_offset{0.0, 0.0, 0.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    FVector destination_offset{0.0, 0.0, 900.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam", meta = (ClampMin = "1.0"))
    float beam_width{95.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam", meta = (ClampMin = "0.0"))
    float flow_speed{2.4f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Energy Beam",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float turbulence{0.72f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam", meta = (ClampMin = "0.0"))
    float emissive_intensity{18.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    FLinearColor core_colour{0.7f, 0.95f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    FLinearColor sheath_colour{0.05f, 0.22f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    bool animation_paused{false};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AEnergyBeamExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AEnergyBeamExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Energy Beam")
    void reverse_direction();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Energy Beam")
    void set_short_beam();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Energy Beam")
    void set_long_beam();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Energy Beam")
    void restart_animation();

    UFUNCTION(BlueprintPure, Category = "Energy Beam")
    float beam_length() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Beam")
    FEnergyBeamSettings settings;
  private:
    void ensure_material();
    void update_beam_transform();
    void apply_settings();

    UPROPERTY(VisibleAnywhere, Category = "Energy Beam")
    TObjectPtr<USceneComponent> root_;

    UPROPERTY(VisibleAnywhere, Category = "Energy Beam")
    TObjectPtr<UStaticMeshComponent> beam_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> beam_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    float animation_time_{0.0f};
};
