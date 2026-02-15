#pragma once

#include "Sandbox/players/DamageableShip.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ShipTrainingTarget.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class AShipTrainingTarget
    : public APawn
    , public IDamageableShip {
    GENERATED_BODY()
  public:
    AShipTrainingTarget();

    // IDamageableShip
    auto apply_damage(int32 damage, AActor const& instigator) -> FShipDamageResult override;

    UPROPERTY(EditAnywhere, Category = "Target")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Target")
    UBoxComponent* collision_box{nullptr};
};
