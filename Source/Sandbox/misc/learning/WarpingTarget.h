#pragma once

#include "Sandbox/core/Cooldown.h"
#include "Sandbox/misc/learning/TestMaterialConfig.h"

#include <GameFramework/Actor.h>
#include <UObject/ObjectMacros.h>
#include <UObject/ObjectPtr.h>
#include <UObject/WeakObjectPtrTemplates.h>

#include "WarpingTarget.generated.h"

class UStaticMeshComponent;
class AWarpVolume;

UCLASS()
class AWarpingTarget : public AActor {
    GENERATED_BODY()
  public:
    AWarpingTarget();

    void Tick(float dt) override;

    auto warp() -> bool;
    UFUNCTION(CallInEditor, Category = "Warping")
    void warp_now();
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void initialise_material();
    auto assert_valid_volume() const -> bool;

    // Visuals
    UPROPERTY(EditAnywhere, Category = "target")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "target")
    FTestMaterialState material_config;

    // Box
    UPROPERTY(EditAnywhere, Category = "target")
    TObjectPtr<AWarpVolume> warp_volume{nullptr};

    // Warping
    UPROPERTY(EditAnywhere, Category = "target")
    bool warp_periodically{true};
    UPROPERTY(EditAnywhere, Category = "target")
    bool warp_on_first_tick{true};
    UPROPERTY(EditAnywhere, Category = "target")
    FCooldown warp_cooldown{10.f};

    // Destruction
    UPROPERTY(EditAnywhere, Category = "target")
    float destroy_after{-1.f};
};
