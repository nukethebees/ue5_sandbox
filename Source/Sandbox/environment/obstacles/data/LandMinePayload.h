#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

#include "Sandbox/combat/effects/data/ExplosionConfig.h"
#include "Sandbox/interaction/collision/data/CollisionContext.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "LandMinePayload.generated.h"

USTRUCT(BlueprintType)
struct FLandMinePayload {
    GENERATED_BODY()

    FLandMinePayload() = default;

    void execute(FCollisionContext context);
    void explode(UWorld& world);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FExplosionConfig explosion_config;
    FVector mine_location{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float detonation_delay{2.0f};
  private:
    FTimerHandle timer_handle{};

    static constexpr auto log{ml::LogMsgMixin<"FLandMinePayload">{}};
};
