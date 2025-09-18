#pragma once

#include "Components/BoxComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/actor_components/PickupComponent.h"
#include "Sandbox/interfaces/PickupOwner.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "PickupActor.generated.h"

namespace ml {
inline static constexpr wchar_t APickupActorLogTag[]{TEXT("APickupActor")};
}

UCLASS()
class SANDBOX_API APickupActor
    : public AActor
    , public IPickupOwner
    , public ml::LogMsgMixin<ml::APickupActorLogTag> {
    GENERATED_BODY()
  public:
    APickupActor();

    // IPickupOwner implementation
    virtual UPrimitiveComponent* get_pickup_collision_component() override;
    virtual bool should_destroy_after_pickup() const override;
    virtual float get_pickup_cooldown() const override;
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    UPickupComponent* pickup_component;

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
