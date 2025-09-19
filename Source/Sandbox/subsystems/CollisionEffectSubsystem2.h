#pragma once

#include <tuple>
#include <utility>

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/data/SpeedBoost.h"

#include "CollisionEffectSubsystem2.generated.h"

namespace ml {
inline static constexpr wchar_t UCollisionEffectSubsystem2LogTag[]{
    TEXT("UCollisionEffectSubsystem2")};
}

template <typename... Args>
using ArrayTuple = std::tuple<TArray<Args>...>;

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

class UCollisionEffectSubsystem2Mixins {
    template <typename Self, typename Payload>
    void add_payload(this Self&& self, Payload&& payload) {
        std::forward<Self>(self).payloads.Add(std::forward<Payload>(payload));
    }
};

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem2
    : public UWorldSubsystem
    , public UCollisionEffectSubsystem2Mixins
    , public ml::LogMsgMixin<ml::UCollisionEffectSubsystem2LogTag> {
    GENERATED_BODY()
  public:
    ArrayTuple<FSpeedBoostPayload, FJumpIncreasePayload> payloads;
};
