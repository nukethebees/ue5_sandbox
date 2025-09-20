#pragma once

#include "Sandbox/data/CollisionContext.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "TimerManager.h"

#include "LandMinePayload.generated.h"

USTRUCT(BlueprintType)
struct FLandMinePayload {
    GENERATED_BODY()

    FLandMinePayload() = default;
    FLandMinePayload(float damage, float radius, float force, FVector location, float delay)
        : damage(damage)
        , explosion_radius(radius)
        , explosion_force(force)
        , mine_location(location)
        , detonation_delay(delay) {}

    void execute(FCollisionContext context);
    void explode(FCollisionContext context);
    FVector calculate_impulse(FVector target_location, float target_distance);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float damage{25.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float explosion_radius{300.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float explosion_force{5000.0f};
    FVector mine_location{FVector::ZeroVector};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float detonation_delay{2.0f};
  private:
    FTimerHandle timer_handle{};

    static constexpr auto log{ml::LogMsgMixin<"FLandMinePayload">{}};
};
