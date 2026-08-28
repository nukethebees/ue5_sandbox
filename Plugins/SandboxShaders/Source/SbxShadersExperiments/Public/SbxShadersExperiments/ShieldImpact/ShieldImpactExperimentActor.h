#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShieldImpactExperimentActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct SBXSHADERSEXPERIMENTS_API FShieldImpactSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Impact")
    FLinearColor base_colour{0.0f, 0.08f, 0.22f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Impact")
    FLinearColor impact_colour{0.15f, 0.85f, 1.0f, 1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Impact")
    FVector impact_centre_local{-1.0, 0.2, 0.15};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.005", ClampMax = "0.5"))
    float preview_radius{0.035f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0"))
    float preview_strength{1.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.1"))
    float pulse_duration{1.8f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0"))
    float repeat_delay{0.45f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.005", ClampMax = "0.5"))
    float pulse_width{0.075f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0"))
    float expansion_speed{0.48f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0"))
    float impact_intensity{18.0f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.1"))
    float fresnel_power{3.5f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float noise_breakup{0.25f};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Shield Impact",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float opacity{0.65f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Impact")
    bool auto_repeat{true};
};

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AShieldImpactExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AShieldImpactExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void Tick(float delta_seconds) override;
    bool ShouldTickIfViewportsOnly() const override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Shield Impact")
    void trigger_impact();

    UFUNCTION(BlueprintCallable, Category = "Shield Impact")
    void trigger_impact_at_local_position(FVector local_impact_centre);

    UFUNCTION(BlueprintCallable, Category = "Shield Impact")
    bool add_impact(FVector local_impact_centre, float radius, float strength);

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Shield Impact")
    void clear_impacts();

    [[nodiscard]] int32 active_impact_count() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield Impact")
    FShieldImpactSettings settings;
  private:
    void ensure_material();
    void apply_settings();
    void apply_impacts();

    struct FActiveImpact {
        FVector centre{FVector::ForwardVector};
        float radius{0.035f};
        float age{0.0f};
        float strength{1.0f};
    };

    UPROPERTY(VisibleAnywhere, Category = "Shield Impact")
    TObjectPtr<UStaticMeshComponent> shield_mesh_;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> shield_material_;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> material_instance_;

    TArray<FActiveImpact> active_impacts_;
    float auto_repeat_elapsed_{0.0f};
    int32 auto_repeat_sequence_{0};
};
