#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/PickupOwner.h"

#include "PickupActor.generated.h"

UCLASS()
class SANDBOX_API APickupActor
    : public AActor
    , public IPickupOwner {
    GENERATED_BODY()
  public:
    APickupActor();

    // IPickupOwner implementation
    virtual UPrimitiveComponent* get_pickup_collision_component() override;
    virtual bool should_destroy_after_pickup() const override;
    virtual float get_pickup_cooldown() const override;
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
    UBoxComponent* collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool destroy_after_pickup{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Pickup",
              meta = (EditCondition = "!destroy_after_pickup"))
    float pickup_cooldown{0.0f};
};
