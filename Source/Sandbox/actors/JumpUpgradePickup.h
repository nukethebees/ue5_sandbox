#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Sandbox/actors/PickupActor.h"

#include "JumpUpgradePickup.generated.h"

class URotatingActorComponent;

UCLASS()
class SANDBOX_API AJumpUpgradePickup : public APickupActor {
    GENERATED_BODY()
  public:
    AJumpUpgradePickup();

    // IPickupOwner implementation
    virtual void on_pickup_effect(AActor* collector) override;
    virtual bool should_destroy_after_pickup() const { return true; }
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    URotatingActorComponent* rotating_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    int32 jump_count_increase{1};
};
