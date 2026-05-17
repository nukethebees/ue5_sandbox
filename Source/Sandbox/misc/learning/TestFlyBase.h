#pragma once

#include "Sandbox/misc/learning/TestMaterialConfig.h"
#include "Sandbox/players/DamageableShip.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "TestFlyBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

struct FHitResult;

class UShipHealthComponent;

UCLASS()
class ATestFlyBase
    : public APawn
    , public IDamageableShip {
  public:
    GENERATED_BODY()

    ATestFlyBase();
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    auto check_if_hit(FVector start, FVector end) -> FHitResult;

    void configure_material();

    // IDamageableShip
    auto apply_damage(ShipDamageContext context) -> FShipDamageResult override;
    auto get_on_killed_delegate() -> FOnKilled& override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fly|Visuals")
    TObjectPtr<UStaticMeshComponent> main_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Fly|Visuals")
    FTestMaterialState material_state;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fly|Collision")
    TObjectPtr<UBoxComponent> main_collision{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fly|Health")
    TObjectPtr<UShipHealthComponent> health{nullptr};
    FOnKilled on_killed;
};
