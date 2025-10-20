#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interaction/collision/interfaces/CollisionOwner.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "PickupActor.generated.h"

UCLASS()
class SANDBOX_API APickupActor
    : public AActor
    , public ICollisionOwner
    , public ml::LogMsgMixin<"APickupActor"> {
    GENERATED_BODY()
  public:
    APickupActor();

    // ICollisionOwner implementation
    virtual UPrimitiveComponent* get_collision_component() override { return collision_component; }
    virtual bool should_destroy_after_collision() const override { return destroy_after_collision; }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
    UBoxComponent* collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    bool destroy_after_collision{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Collision",
              meta = (EditCondition = "!destroy_after_collision"))
    float collision_cooldown{0.0f};
};
