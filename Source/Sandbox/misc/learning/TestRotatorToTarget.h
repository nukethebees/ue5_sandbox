#pragma once

#include "Sandbox/misc/learning/TestMaterialConfig.h"

#include <GameFramework/Actor.h>
#include <Math/MathFwd.h>
#include <UObject/ObjectMacros.h>
#include <UObject/ObjectPtr.h>
#include <UObject/WeakObjectPtrTemplates.h>

#include "TestRotatorToTarget.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class ATestRotatorToTarget : public AActor {
    GENERATED_BODY()
  public:
    ATestRotatorToTarget();

    void Tick(float dt) override;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void configure_material();
    void look_at_target(float dt);

    // Visuals
    UPROPERTY(EditAnywhere, Category = "target")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "target")
    FTestMaterialState material_config;

    UPROPERTY(EditAnywhere, Category = "target")
    TWeakObjectPtr<AActor> target{nullptr};
    UPROPERTY(EditAnywhere, Category = "target")
    float turning_speed_deg_per_s{45.f};
};
