#pragma once

#include <optional>
#include <tuple>
#include <utility>

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/tuple.h"
#include "Subsystems/WorldSubsystem.h"

#include "CollisionEffectSubsystem2.generated.h"

namespace ml {
inline static constexpr wchar_t UCollisionEffectSubsystem2LogTag[]{
    TEXT("UCollisionEffectSubsystem2")};

template <typename T>
using CollisionArray = TArray<T>;

template <typename... Args>
using ArrayTuple = std::tuple<CollisionArray<Args>...>;

template <typename T, typename Tuple>
decltype(auto) ArrayGet(Tuple&& tup) {
    return std::get<CollisionArray<T>>(std::forward<Tuple>(tup));
}

template <typename T, typename Subsystem>
constexpr auto tuple_array_index_v =
    tuple_type_index<CollisionArray<T>, typename std::remove_cvref_t<Subsystem>::PayloadsT>::value;
}

USTRUCT(BlueprintType)
struct FSpeedBoostPayload {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSpeedBoost speed_boost{};

    FSpeedBoostPayload() = default;
    FSpeedBoostPayload(FSpeedBoost boost)
        : speed_boost(boost) {}
};

USTRUCT(BlueprintType)
struct FJumpIncreasePayload {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 jump_increase{1};
};

USTRUCT(BlueprintType)
struct FPayloadIndex {
    GENERATED_BODY()

    FPayloadIndex() = default;
    FPayloadIndex(uint8 tag, uint8 i)
        : type_tag(tag)
        , array_index(i) {}

    uint8 type_tag{0};
    uint8 array_index{0};
};

USTRUCT(BlueprintType)
struct FActorPayloadIndexes {
    GENERATED_BODY()

    FActorPayloadIndexes() = default;

    TArray<FPayloadIndex> indexes;
};

class UCollisionEffectSubsystem2Mixins {
  public:
    template <typename Self, typename Payload>
    void add_payload(this Self&& self, AActor* actor, Payload&& payload) {
        auto const actor_index{self.register_actor(actor)};
        if (!actor_index) {
            return;
        }

        auto& payload_array{ml::ArrayGet<Payload>(std::forward<Self>(self).payloads)};
        payload_array.Add(std::forward<Payload>(payload));

        constexpr auto array_index{ml::tuple_array_index_v<Payload, Self>};
    }

    template <typename Self>
    std::optional<int32> register_actor(this Self&& self, AActor* actor) {
        if (!actor) {
            return std::nullopt;
        }

        if (auto const* i{self.actor_ids.Find(actor)}) {
            return *i;
        }

        auto* collision_owner{Cast<ICollisionOwner>(actor)};
        if (!collision_owner) {
            return std::nullopt;
        }

        auto* collision_comp{collision_owner->get_collision_component()};
        if (!collision_comp) {
            return std::nullopt;
        }

        collision_comp->OnComponentBeginOverlap.AddDynamic(
            &self, &std::remove_cvref_t<Self>::handle_collision_event);

        auto const i{self.actor_ids.Add(actor)};
        std::forward<Self>(self).actor_payload_indexes.AddDefaulted();
        return i;
    }
};

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem2
    : public UWorldSubsystem
    , public UCollisionEffectSubsystem2Mixins
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystem2LogTag> {
    GENERATED_BODY()
    friend class UCollisionEffectSubsystem2Mixins;
  public:
    using PayloadsT = ml::ArrayTuple<FSpeedBoostPayload, FJumpIncreasePayload>;

    void handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex,
                                bool bFromSweep,
                                FHitResult const& SweepResult);
  private:
    PayloadsT payloads;
    TMap<AActor*, int32> actor_ids;
    TArray<FActorPayloadIndexes> actor_payload_indexes;
};
