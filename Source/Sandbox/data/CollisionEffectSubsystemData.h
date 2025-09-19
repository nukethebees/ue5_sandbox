#pragma once

#include <tuple>

#include "CoreMinimal.h"
#include "Sandbox/data/PayloadIndex.h"
#include "Sandbox/mixins/CollisionEffectSubsystemDataMixins.hpp"
#include "Sandbox/mixins/log_msg_mixin.hpp"

namespace ml {
inline static constexpr wchar_t UCollisionEffectSubsystemDataLogTag[]{
    TEXT("UCollisionEffectSubsystemData")};
}

template <typename... Types>
class UCollisionEffectSubsystemData
    : public UCollisionEffectSubsystemDataMixins
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystemDataLogTag> {
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
