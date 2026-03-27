#pragma once

#include "Sandbox/players/DamageableShip.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ShipTrainingTarget.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;

UENUM()
enum class EShipTrainingTargetMovementMode : uint8 {
    stationary,
    move_forward,
    sine,
    orbit_yz,
};

UCLASS()
class AShipTrainingTarget
    : public APawn
    , public IDamageableShip {
    GENERATED_BODY()
  public:
    AShipTrainingTarget();

    void Tick(float dt) override;

    // IDamageableShip
    auto apply_damage(ShipDamageContext context) -> FShipDamageResult override;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Target")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Target")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "Target")
    UNiagaraSystem* death_particles{nullptr};

    UPROPERTY(EditAnywhere, Category = "Target")
    EShipTrainingTargetMovementMode movement_mode{EShipTrainingTargetMovementMode::stationary};
    UPROPERTY(EditAnywhere, Category = "Target")
    float speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "Target|Sine")
    float frequency{1.f};
    UPROPERTY(EditAnywhere, Category = "Target|Sine")
    FVector3d sine_amplitude{0.f, 0.f, 1.f};
    UPROPERTY(EditAnywhere, Category = "Target|Sine")
    float orbit_radius{50.f};

    UPROPERTY(VisibleAnywhere, Category = "Target")
    FVector3d pos{FVector3d::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Target")
    FVector3d pos_offset{FVector3d::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Target|Sine")
    float angular_frequency{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Target|Sine")
    float wt{0.f};
};
