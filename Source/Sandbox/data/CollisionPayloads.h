#pragma once

#include "CoreMinimal.h"
#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/data/CollisionContext.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

#include "CollisionPayloads.generated.h"

USTRUCT(BlueprintType)
struct FSpeedBoostPayload {
    GENERATED_BODY()

    FSpeedBoostPayload() = default;
    FSpeedBoostPayload(FSpeedBoost boost)
        : speed_boost(boost) {}

    void execute(FCollisionContext context);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSpeedBoost speed_boost{};
};

USTRUCT(BlueprintType)
struct FJumpIncreasePayload {
    GENERATED_BODY()

    FJumpIncreasePayload() = default;
    FJumpIncreasePayload(int32 inc)
        : jump_count_increase(inc) {}

    void execute(FCollisionContext context);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 jump_count_increase{1};
};

USTRUCT(BlueprintType)
struct FCoinPayload {
    GENERATED_BODY()

    FCoinPayload() = default;
    FCoinPayload(int32 x)
        : value(x) {}

    void execute(FCollisionContext context);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 value{1};
};
