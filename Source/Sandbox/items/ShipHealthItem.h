#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipHealthItem.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;
class UMaterialInterface;

class URotatingActorComponent;
class UShipHealthItemConfig;

UENUM(BlueprintType)
enum class EShipHealthItemType : uint8 { Silver, Gold, SuperSilver };

UCLASS()
class AShipHealthItem : public AActor {
    GENERATED_BODY()

    AShipHealthItem();

    void set_config(UShipHealthItemConfig const& config);
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform);

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
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UMaterialInterface* material{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 health{50};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    EShipHealthItemType type{EShipHealthItemType::Silver};
};
