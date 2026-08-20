#pragma once

#include "SandboxGameShared/core/destruction/DestructionManagerSubsystem.h"
#include "ShooterGame/environment/obstacles/LandMinePayload.h"
#include "ShooterGame/interaction/CollisionContext.h"
#include "ShooterGame/items/SpeedBoost.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/players/CoinCollectorActorComponent.h"
#include "ShooterGame/players/MyCharacter.h"
#include "ShooterGame/players/SpeedBoostComponent.h"

#include "CoreMinimal.h"

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
