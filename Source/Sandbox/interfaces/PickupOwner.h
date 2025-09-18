#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Interface.h"

#include "PickupOwner.generated.h"

UINTERFACE(MinimalAPI)
class UPickupOwner : public UInterface {
    GENERATED_BODY()
};

class IPickupOwner {
    GENERATED_BODY()
  public:
    virtual UPrimitiveComponent* get_pickup_collision_component() = 0;
    virtual bool should_destroy_after_pickup() const = 0;
    virtual float get_pickup_cooldown() const { return 0.0f; }

    virtual void on_pre_pickup_effect(AActor* collector) {}
    virtual void on_post_pickup_effect(AActor* collector) {}
};
