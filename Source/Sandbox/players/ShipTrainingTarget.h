#pragma once

#include "Sandbox/players/DamageableShip.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ShipTrainingTarget.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UENUM()
enum class EShipTrainingTargetMovementMode : uint8 { stationary, move_forward };

UCLASS()
class AShipTrainingTarget
    : public APawn
    , public IDamageableShip {
    GENERATED_BODY()
  public:
    AShipTrainingTarget();

    void Tick(float dt) override;

    // IDamageableShip
    auto apply_damage(int32 damage, AActor const& instigator) -> FShipDamageResult override;
  protected:
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Target")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Target")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "Target")
    EShipTrainingTargetMovementMode movement_mode{EShipTrainingTargetMovementMode::stationary};
    UPROPERTY(EditAnywhere, Category = "Target")
    FVector3d velocity{FVector3d::ZeroVector};
};
