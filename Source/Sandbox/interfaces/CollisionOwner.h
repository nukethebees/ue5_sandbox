#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Interface.h"

#include "CollisionOwner.generated.h"

UINTERFACE(MinimalAPI)
class UCollisionOwner : public UInterface {
    GENERATED_BODY()
};

class ICollisionOwner {
    GENERATED_BODY()
  public:
    virtual UPrimitiveComponent* get_collision_component() = 0;
    virtual bool should_destroy_after_collision() const = 0;

    virtual void on_pre_collision_effect(AActor& other_actor) {}
    virtual void on_collision_effect(AActor& other_actor) {}
    virtual void on_post_collision_effect(AActor& other_actor) {}
};
