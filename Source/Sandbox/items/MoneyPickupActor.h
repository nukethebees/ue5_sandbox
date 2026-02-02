#pragma once

#include "Sandbox/interaction/CollisionOwner.h"
#include "Sandbox/interaction/Describable.h"
#include "Sandbox/items/MoneyPickupPayload.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MoneyPickupActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class SANDBOX_API AMoneyPickupActor
    : public AActor
    , public ICollisionOwner
    , public IDescribable {
    GENERATED_BODY()
  public:
    AMoneyPickupActor();

    virtual UPrimitiveComponent* get_collision_component() override;
    virtual bool should_destroy_after_collision() const override;

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Money"))};
        return desc;
    }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Money")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Money")
    UBoxComponent* collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money")
    FMoneyPickupPayload money_payload{1};
};
