#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldComponent.h"

#include "GpuStarfieldExperimentActor.generated.h"

UCLASS(Blueprintable)
class SBXSHADERSEXPERIMENTS_API AGpuStarfieldExperimentActor final : public AActor {
    GENERATED_BODY()
  public:
    AGpuStarfieldExperimentActor();

    void OnConstruction(FTransform const& transform) override;
    void PostRegisterAllComponents() override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "GPU Starfield")
    void apply_settings();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldSettings settings;
  private:
    UPROPERTY(VisibleAnywhere, Category = "GPU Starfield")
    TObjectPtr<UGpuStarfieldComponent> starfield_component_;
};
