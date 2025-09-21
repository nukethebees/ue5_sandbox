#pragma once

#include "CoreMinimal.h"
#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/data/collision/CollisionContext.h"
#include "Sandbox/data/LandMinePayload.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

struct FSpeedBoostPayload {
    FSpeedBoostPayload() = default;
    FSpeedBoostPayload(FSpeedBoost boost)
        : speed_boost(boost) {}

    void execute(FCollisionContext context);

    FSpeedBoost speed_boost{};
};

struct FJumpIncreasePayload {
    FJumpIncreasePayload() = default;
    FJumpIncreasePayload(int32 inc)
        : jump_count_increase(inc) {}

    void execute(FCollisionContext context);

    int32 jump_count_increase{1};
};

struct FCoinPayload : public ml::LogMsgMixin<"FCoinPayload"> {
    FCoinPayload() = default;
    FCoinPayload(int32 x)
        : value(x) {}

    void execute(FCollisionContext context);

    int32 value{1};
};
