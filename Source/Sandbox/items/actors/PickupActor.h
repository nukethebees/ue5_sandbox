#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/interaction/CollisionOwner.h"
#include "Sandbox/logging/LogMsgMixin.hpp"

#include "PickupActor.generated.h"

class UBoxComponent;

UCLASS()
class SANDBOX_API APickupActor
    : public AActor
    , public ICollisionOwner
    , public ml::LogMsgMixin<"APickupActor"> {
    GENERATED_BODY()
  public:
    APickupActor();

    // ICollisionOwner implementation
    virtual UPrimitiveComponent* get_collision_component() override;
    virtual bool should_destroy_after_collision() const override;
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
