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

template <typename... Types>
class UCollisionEffectSubsystemData
    : public UCollisionEffectSubsystemDataMixins
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystem2LogTag> {
    friend class UCollisionEffectSubsystemDataMixins;
  public:
    using PayloadsT = ml::ArrayTuple<Types...>;
    static constexpr std::size_t N_TYPES = sizeof...(Types);
  private:
    PayloadsT payloads;
    TArray<FActorPayloadIndexes> actor_payload_indexes;
    TArray<TWeakObjectPtr<AActor>> actors;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> collision_boxes;

    TMap<AActor*, int32> actor_ids;
    TMap<UPrimitiveComponent*, int32> collision_ids{};
};

class UCollisionEffectSubsystemMixins {
    // Create forwarding functions
#define FORWARDING_FN(FN_NAME, ...)                                                              \
    template <typename Self, typename... Args>                                                   \
    decltype(auto) FN_NAME(this Self&& self, Args... args) {                                     \
        return std::forward<Self>(self).data_.FN_NAME(std::forward<Args>(args)... __VA_OPT__(, ) \
                                                          __VA_ARGS__);                          \
    }
  public:
    FORWARDING_FN(add_payload, &self);
    FORWARDING_FN(register_actor, &self)
  protected:
    FORWARDING_FN(handle_collision_event_)

#undef FORWARDING_FN
};

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem2
    : public UWorldSubsystem
    , public UCollisionEffectSubsystemMixins {
    GENERATED_BODY()
    friend class UCollisionEffectSubsystemMixins;
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
    UCollisionEffectSubsystemData<FSpeedBoostPayload, FJumpIncreasePayload> data_{};
};
