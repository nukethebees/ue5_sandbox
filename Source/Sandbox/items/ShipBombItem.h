#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipBombItem.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

class URotatingActorComponent;

UCLASS()
class AShipBombItem : public AActor {
    GENERATED_BODY()

    AShipBombItem();
  protected:
    void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                          AActor* other_actor,
                          UPrimitiveComponent* other_comp,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    URotatingActorComponent* rotator{nullptr};
};
