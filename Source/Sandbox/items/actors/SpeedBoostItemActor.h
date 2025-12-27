#pragma once

#include "CoreMinimal.h"
#include "Sandbox/items/actors/PickupActor.h"

#include "SpeedBoostItemActor.generated.h"

class StaticMeshComponent;

class USpeedBoostItemComponent;

UCLASS()
class SANDBOX_API ASpeedBoostItemActor : public APickupActor {
    GENERATED_BODY()
  public:
    ASpeedBoostItemActor();
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    USpeedBoostItemComponent* speed_boost_component{};
};
