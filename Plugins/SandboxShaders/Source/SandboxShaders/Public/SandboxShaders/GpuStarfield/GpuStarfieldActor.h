#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SandboxShaders/GpuStarfield/GpuStarfieldComponent.h"

#include "GpuStarfieldActor.generated.h"

UCLASS(Blueprintable)
class SANDBOXSHADERS_API AGpuStarfieldActor : public AActor {
    GENERATED_BODY()
  public:
    AGpuStarfieldActor();

    void OnConstruction(FTransform const& transform) override;
    void PostRegisterAllComponents() override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "GPU Starfield")
    void apply_settings();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPU Starfield")
    FGpuStarfieldSettings settings;
  protected:
    UPROPERTY(VisibleAnywhere, Category = "GPU Starfield")
    TObjectPtr<UGpuStarfieldComponent> starfield_component_;
};
