#pragma once

#include "CoreMinimal.h"

#include "Sandbox/items/actors/PickupActor.h"

#include "JumpUpgradePickup.generated.h"

class UStaticMeshComponent;

class URotatingActorComponent;

UCLASS()
class SANDBOX_API AJumpUpgradePickup : public APickupActor {
    GENERATED_BODY()
  public:
    AJumpUpgradePickup();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    URotatingActorComponent* rotating_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    int32 jump_count_increase{1};
};
