#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

#include "Sandbox/interaction/collision/interfaces/CollisionOwner.h"
#include "Sandbox/items/data/MoneyPickupPayload.h"

#include "MoneyPickupActor.generated.h"

UCLASS()
class SANDBOX_API AMoneyPickupActor
    : public AActor
    , public ICollisionOwner {
    GENERATED_BODY()
  public:
    AMoneyPickupActor();

    virtual UPrimitiveComponent* get_collision_component() override { return collision_component; }
    virtual bool should_destroy_after_collision() const override { return true; }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Money")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Money")
    UBoxComponent* collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money")
    FMoneyPickupPayload money_payload{1};
};
