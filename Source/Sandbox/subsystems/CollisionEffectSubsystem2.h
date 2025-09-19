#pragma once

#include <tuple>

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/data/CollisionPayloads.h"
#include "Sandbox/data/PayloadIndex.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/mixins/CollisionEffectSubsystem2Mixins.hpp"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Subsystems/WorldSubsystem.h"

#include "CollisionEffectSubsystem2.generated.h"

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem2
    : public UWorldSubsystem
    , public UCollisionEffectSubsystem2Mixins
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystem2LogTag> {
    GENERATED_BODY()
    friend class UCollisionEffectSubsystem2Mixins;
  public:
    using PayloadsT = ml::ArrayTuple<FSpeedBoostPayload, FJumpIncreasePayload>;
    static constexpr std::size_t N_TYPES = std::tuple_size_v<PayloadsT>;
  public:
    UFUNCTION()
    void handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex,
                                bool bFromSweep,
                                FHitResult const& SweepResult) {
        return handle_collision_event_(
            OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
    }
  private:
    PayloadsT payloads;
    TArray<FActorPayloadIndexes> actor_payload_indexes;
    TArray<TWeakObjectPtr<AActor>> actors;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> collision_boxes;

    TMap<AActor*, int32> actor_ids;
    TMap<UPrimitiveComponent*, int32> collision_ids{};
};
