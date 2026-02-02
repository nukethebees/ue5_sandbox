#pragma once

#include "CoreMinimal.h"

#include "Sandbox/core/destruction/subsystems/DestructionManagerSubsystem.h"
#include "Sandbox/environment/obstacles/data/LandMinePayload.h"
#include "Sandbox/interaction/CollisionContext.h"
#include "Sandbox/items/data/SpeedBoost.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/players/playable/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/players/playable/actor_components/SpeedBoostComponent.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"

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
